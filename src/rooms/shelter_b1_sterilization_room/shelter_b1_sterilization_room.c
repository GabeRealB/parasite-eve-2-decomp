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
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
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

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The "%" suffix appended to a formatted percentage.
static u8 Telephone_Data_80181A78[];

/// UI descriptor of the help-line box the "Play Data" panels open beside their
/// lists.
static UiObjectDesc Telephone_Data_80181C90;

/// Task descriptor table used by the cutscene runner
/// `_roomCutsceneTask`, which spawns entry 1 and
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

/// List state of the menu `shelterB1SterilizationRoomTelephoneMenuTask` runs.
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

/// Area records `_roomCutsceneTask` applies when it
/// advances game flag nibble 0 from 2 to 3 in one particular view.

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_160600_80135AC0[];
extern EvsCommand D_actor_160600_80135D78[];
extern EvsCommand D_actor_160600_80136258[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\0 "
#include "../../shared/telephone.h"

static void _shelterB1SterilizationRoomInitializeState(Task* task);
static void _shelterB1SterilizationRoomInitializeActorObstacle(s32 unused);
static void _shelterB1SterilizationRoomUpdateState(Task* task);
static void _shelterB1SterilizationRoomUpdateEventActorVisibility(void);
static void _shelterB1SterilizationRoomRebuildActorObstacle(const GfxCoord* modelRoot, const s16 roomOffset[3]);
static void _shelterB1SterilizationRoomCaptureCrossfadeState(Task* task);
static void _shelterB1SterilizationRoomWaitCrossfadeViewState(Task* task);

static s32  _shelterB1SterilizationRoomHandleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _shelterB1SterilizationRoomHandleCommandMessage(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32  _shelterB1SterilizationRoomRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelterB1SterilizationRoomResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB1SterilizationRoomSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unused);
static void _shelterB1SterilizationRoomScriptedPresentationTask(Task* task);

static AnimationSet _gShelterB1SterilizationRoomAnimation07C68;
static AnimationSet _gShelterB1SterilizationRoomAnimation07E1C;
static AnimationSet _gShelterB1SterilizationRoomAnimation0853C;
static AnimationSet _gShelterB1SterilizationRoomAnimation086CC;
static AnimationSet _gShelterB1SterilizationRoomAnimation09350;
static AnimationSet _gShelterB1SterilizationRoomAnimation0A858;
static AnimationSet _gShelterB1SterilizationRoomAnimation0AF1C;
static void         _shelterB1SterilizationRoomDoorPassageTask(Task* task);
static void         _shelterB1SterilizationRoomTrapDamageTask(Task* task);
static void         _shelterB1SterilizationRoomCrossfadeTask(Task* task);

/// Placement and progress selectors of the room's event actor and its obstacle.
enum {
    SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_PLACEMENT = 0,
    SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_VARIANT   = 5,
    SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_ACTIVE    = 1,
    SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_FACE_COUNT   = 4,
    SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_VERTEX_COUNT = 8,
};

/// Door room actions 3..10 map to zero-based destination entries 0..7.
enum { SHELTER_B1_STERILIZATION_ROOM_FIRST_DOOR_ACTION = 3 };

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
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b1_sterilization_room_80184E40[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1SterilizationRoomResolveTransitionMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1SterilizationRoomRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1SterilizationRoomHandleActionMessage },
    { ROOM_MESSAGE_COMMAND, _shelterB1SterilizationRoomHandleCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB1SterilizationRoomSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_sterilization_room_80184E70 = { { { TASK_BODY_NONE, 192 } }, _shelterB1SterilizationRoomScriptedPresentationTask, { .value = 0 } };

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
    { { { TASK_BODY_NONE, 192 } }, _shelterB1SterilizationRoomCrossfadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomTrapEscapePromptTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterB1SterilizationRoomDoorPassageTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomSwitchRoomTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomDoorNoticeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterB1SterilizationRoomTrapDamageTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomResetSpawnMaskTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomTrapDialogueTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, shelterB1SterilizationRoomSceneDialogueTask, { .value = 0 } },
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

void shelterB1SterilizationRoomTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// The three states of the room's main task, run by
/// `shelterB1SterilizationRoomTask`: set-up, the per-frame
/// handler and the kill.
static const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4 = {
    {
        _shelterB1SterilizationRoomInitializeState,
        _shelterB1SterilizationRoomUpdateState,
        taskKill,
    },
};

/// Registers the room receiver and starts its first variant-5 actor event when eligible.
///
/// State 0 installs the message table and room slot, applies saved area changes,
/// and selects the companion-dependent event script. It initializes the actor
/// obstacle, starts placed soldier 0 smoking, disables trigger 22 except in
/// the event-without-companion state, and rearms event spawns on variant 1.
/// Spawn arguments are unused. Requires live session, player, placement and
/// event-script resources; advances to the room's update state.
static void _shelterB1SterilizationRoomInitializeState(Task* task)
{
    enum {
        ENTRY_EVENT_UNSEEN             = 0,
        ENTRY_EVENT_WITHOUT_COMPANION  = 1,
        ENTRY_EVENT_WITH_COMPANION     = 2,
        PARKING_PROGRESS_EVENT_STARTED = 3,
        COMPANION_REMARK_PRESENT       = 1,
        COMPANION_REMARK_ABSENT        = 2,
        COMPANION_EVENT_SCHEDULE       = 8,
        COMPANION_PLACEMENT_VARIANT    = 6,
        FIRST_ENTRY_VARIANT            = 1,
        EVENT_TRIGGER_INDEX            = 22,
    };
    Task* placedActor;

    task->msgTable = D_shelter_b1_sterilization_room_80184E40;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    // Start the first variant-5 event before installing its actor obstacle.
    if (gGameSession->location.loc.variant == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_VARIANT && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == ENTRY_EVENT_UNSEEN) {
        gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, PARKING_PROGRESS_EVENT_STARTED);
        areaApplySavedUpdates(D_shelter_b1_sterilization_room_8018C334);
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, COMPANION_REMARK_PRESENT);
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, ENTRY_EVENT_WITH_COMPANION);
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_EVENT_SCHEDULE);
            evsStartScriptWithSkip(D_actor_160600_80135D78, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_160600_80136258);
            areaSetPlacementVariant(&gGameSession->location.loc, COMPANION_PLACEMENT_VARIANT, AREA_VARIANT_RESET_ALWAYS);
        } else {
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, COMPANION_REMARK_ABSENT);
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE, ENTRY_EVENT_WITHOUT_COMPANION);
            evsStartScriptWithSkip(D_actor_160600_80135AC0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_160600_80136258);
        }
    }
    _shelterB1SterilizationRoomInitializeActorObstacle(0);
    if (gGameSession->location.loc.variant == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_VARIANT) {
        placedActor = sceneFindPlacedActor(SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_PLACEMENT);
        if (placedActor != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_COMMAND_MESSAGE_APPLY, &D_shelter_b1_sterilization_room_80184E7C, 0);
        }
    }
    if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) != ENTRY_EVENT_WITHOUT_COMPANION) {
        WorldCollisionTrigger* eventTrigger;

        eventTrigger         = &D_shelter_b1_sterilization_room_8018B8A8[EVENT_TRIGGER_INDEX];
        eventTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    if (gGameSession->location.loc.variant == FIRST_ENTRY_VARIANT) {
        taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_RESET_SPAWN_MASK, 0, 0);
    }
    task->state++;
}

/// Routes room actions to the trap sequence, escape prompt, door notices and passages.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request;
/// only actionId is read, and no pointer is retained. Actions 1/2 start the trap
/// or offer escape; door actions 3..10 select zero-based door entries 0..7. Actions
/// 5/8 latch their notes, 4/9 select progress-dependent scene dialogue, and
/// 3/6/7/10 show the shared notice. Other actions do nothing. The task, message
/// ID and second payload are unused; returns zero, which the sender discards.
/// Requires the room overlay and selected CAP/script resources to remain live.
static s32 _shelterB1SterilizationRoomHandleActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    /// Selects the one-time note for an action, writing capCommand and noteMask.
    ///
    /// Captures this function's two s32 locals and constants. Evaluates the
    /// actionId argument once; use as a standalone statement inside a block.
#define SHELTER_B1_STERILIZATION_ROOM_SELECT_DOOR_NOTE(actionId) \
    switch (actionId) {                                          \
        case ACTION_NOTE_DOOR_5:                                 \
            capCommand = CAP_DOOR_5_NOTE;                        \
            noteMask   = DOOR_5_NOTE_BIT;                        \
            break;                                               \
        case ACTION_NOTE_DOOR_8:                                 \
            capCommand = CAP_DOOR_8_NOTE;                        \
            noteMask   = DOOR_8_NOTE_BIT;                        \
            break;                                               \
        default:                                                 \
            capCommand = CAP_NO_DOOR_NOTE;                       \
            noteMask   = NO_DOOR_NOTE_MASK;                      \
            break;                                               \
    }
    enum {
        ACTION_START_TRAP                = 1,
        ACTION_ESCAPE_PROMPT             = 2,
        ACTION_NOTICE_DOOR_3             = 3,
        ACTION_SCENE_DOOR_4              = 4,
        ACTION_NOTE_DOOR_5               = 5,
        ACTION_NOTICE_DOOR_6             = 6,
        ACTION_NOTICE_DOOR_7             = 7,
        ACTION_NOTE_DOOR_8               = 8,
        ACTION_SCENE_DOOR_9              = 9,
        ACTION_NOTICE_DOOR_10            = 10,
        CAP_NO_TRAP_NOTICE               = 0x16,
        CAP_ESCAPED_TRAP_NOTICE          = 0x17,
        CAP_TRAP_BLOCKS_DOOR             = 0xA,
        CAP_DOOR_5_NOTE                  = 5,
        CAP_DOOR_8_NOTE                  = 3,
        DOOR_5_NOTE_BIT                  = 2,
        DOOR_8_NOTE_BIT                  = 8,
        NO_DOOR_NOTE_MASK                = 0xFF,
        CAP_NO_DOOR_NOTE                 = 0,
        SCENE_DOOR_9_FIRST               = 1,
        SCENE_DOOR_9_REPEAT              = 2,
        SCENE_DOOR_4_BEFORE_ESCAPE       = 3,
        SCENE_DOOR_4_AFTER_ESCAPE_FIRST  = 4,
        SCENE_DOOR_4_AFTER_ESCAPE_REPEAT = 5,
        ACTION_SCENE_UNSEEN              = 0,
        TRAP_DIALOGUE_NOT_STARTED        = 0,
        ACTION_SCENE_FIRST_STARTED       = 1,
        ACTION_SCENE_POST_ESCAPE_STARTED = 2,
        TRAP_DORMANT                     = 0,
        TRAP_TRIGGERED                   = 1,
        TRAP_RUNNING                     = 0,
        TRAP_STOPPED                     = 1,
    };
    s32 capCommand;
    s32 noteMask;
    s32 shownNotes;

    switch (request->actionId) {
        case ACTION_START_TRAP:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_DORMANT) {
                if (gameFlagGetNibble(GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN) != 0) {
                    evsStartScriptWithSkip(D_shelter_b1_sterilization_room_8018873C, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                           D_shelter_b1_sterilization_room_80188AB4);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED, TRAP_TRIGGERED);
                }
            }
            break;
        case ACTION_ESCAPE_PROMPT:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_TRIGGERED) {
                if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_RUNNING) {
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_ESCAPE_PROMPT, 0, 0);
                } else {
                    capRunCommandWithTransition(CAP_ESCAPED_TRAP_NOTICE);
                }
            } else {
                capRunCommandWithTransition(CAP_NO_TRAP_NOTICE);
            }
            break;
        // Show each door note once, then request the corresponding passage.
        case ACTION_NOTE_DOOR_5:
        case ACTION_NOTE_DOOR_8:
            SHELTER_B1_STERILIZATION_ROOM_SELECT_DOOR_NOTE(request->actionId);
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_DORMANT || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_STOPPED) {
                shownNotes = gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_NOTES_SHOWN);
                if (capCommand != CAP_NO_DOOR_NOTE && !(shownNotes & noteMask)) {
                    capRunCommandWithTransition(capCommand);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_NOTES_SHOWN, shownNotes | noteMask);
                }
                taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_PASSAGE, request->actionId - SHELTER_B1_STERILIZATION_ROOM_FIRST_DOOR_ACTION, 0);
            } else {
                capRunCommandWithTransition(CAP_TRAP_BLOCKS_DOOR);
            }
            break;
        case ACTION_NOTICE_DOOR_3:
        case ACTION_NOTICE_DOOR_6:
        case ACTION_NOTICE_DOOR_7:
        case ACTION_NOTICE_DOOR_10:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_DORMANT || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_STOPPED) {
                taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_NOTICE, request->actionId - SHELTER_B1_STERILIZATION_ROOM_FIRST_DOOR_ACTION, 0);
            } else {
                capRunCommandWithTransition(CAP_TRAP_BLOCKS_DOOR);
            }
            break;
        case ACTION_SCENE_DOOR_9:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_DORMANT || gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_STOPPED) {
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_149) == TRAP_DIALOGUE_NOT_STARTED) {
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_DOOR_9_FIRST, 0);
                } else {
                    if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION9_SCENE) == ACTION_SCENE_UNSEEN) {
                        taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_DOOR_9_REPEAT, 0);
                        gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION9_SCENE, ACTION_SCENE_FIRST_STARTED);
                    }
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_PASSAGE, request->actionId - SHELTER_B1_STERILIZATION_ROOM_FIRST_DOOR_ACTION, 0);
                }
            }
            break;
        case ACTION_SCENE_DOOR_4:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_RUNNING) {
                if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE) == ACTION_SCENE_UNSEEN) {
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_DOOR_4_BEFORE_ESCAPE, 0);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, ACTION_SCENE_FIRST_STARTED);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE) < ACTION_SCENE_POST_ESCAPE_STARTED) {
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14A) == TRAP_DIALOGUE_NOT_STARTED) {
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_DOOR_4_AFTER_ESCAPE_FIRST, 0);
                } else {
                    taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_DOOR_4_AFTER_ESCAPE_REPEAT, 0);
                }
                gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, ACTION_SCENE_POST_ESCAPE_STARTED);
            }
            taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_PASSAGE, request->actionId - SHELTER_B1_STERILIZATION_ROOM_FIRST_DOOR_ACTION, 0);
            break;
    }
    return 0;
}
#undef SHELTER_B1_STERILIZATION_ROOM_SELECT_DOOR_NOTE

/// Routes CAP room commands to cutscenes, trap dialogue and player presentation.
///
/// Handles `ROOM_MESSAGE_COMMAND`: 17 starts the first CAP event or repeat
/// cutscene, 4/19 select the two trap-dialogue routes, 14 runs the trap exchange,
/// and 12/13 run presentation in room 3. Command 21 passes through to CAP;
/// other selectors do nothing. The task, message ID and second payload are
/// unused. Normally returns zero; the first-event branch has no defined result,
/// so callers must discard it. The shared cutscene record is borrowed through
/// playback; requests must not overlap its use. Keep the room resources loaded.
static s32 _shelterB1SterilizationRoomHandleCommandMessage(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_DIALOGUE           = 4,
        SHELTER_B1_STERILIZATION_ROOM_COMMAND_ALTERNATE_TRAP_DIALOGUE = 0x13,
        COMMAND_CUTSCENE                                              = 0x11,
        COMMAND_CAP_21                                                = 0x15,
        COMMAND_TRAP_EXCHANGE                                         = 0xE,
        COMMAND_PRESENTATION_12                                       = 0xC,
        COMMAND_PRESENTATION_13                                       = 0xD,
        CAP_FIRST_CUTSCENE                                            = 0x18,
        SCENE_REPEAT_TRAP_EXCHANGE                                    = 0xB,
        PRESENTATION_ROOM                                             = 3,
        CUTSCENE_VIEW                                                 = 0x13,
        CUTSCENE_CAP_SLOT                                             = 1,
        CUTSCENE_CAP_FILE                                             = 2,
        CUTSCENE_SKIP_DISABLED                                        = 0,
        CUTSCENE_FOLLOWUP_CAP_COMMAND                                 = 6,
        CUTSCENE_START_SOUND                                          = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0xC),
        CUTSCENE_END_SOUND                                            = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0xF),
        CUTSCENE_SCENE_SOUND                                          = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0xD),
        CUTSCENE_AFTER_SCENE_SOUND                                    = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0xE),
        SCENE_NOT_STARTED                                             = 0,
        SCENE_FIRST_STARTED                                           = 1,
        SCENE_REPEAT_STARTED                                          = 2,
        TRAP_TRIGGERED                                                = 1,
        TRAP_RUNNING                                                  = 0,
    };
    if (command == COMMAND_CUTSCENE) {
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_FIRST_SCENE) == SCENE_NOT_STARTED) {
            gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_FIRST_SCENE, SCENE_FIRST_STARTED);
            capSpawnEventIfIdle(CAP_FIRST_CUTSCENE, CAP_EVENT_PAUSE_ACTORS);
            // This branch leaves the result register unspecified; callers discard it.
            return;
        }
        // Reuse the overlay-owned cutscene record for subsequent requests.
        D_shelter_b1_sterilization_room_8018C344.view            = CUTSCENE_VIEW;
        D_shelter_b1_sterilization_room_8018C344.capSlot         = CUTSCENE_CAP_SLOT;
        D_shelter_b1_sterilization_room_8018C344.capFile         = CUTSCENE_CAP_FILE;
        D_shelter_b1_sterilization_room_8018C344.skipScene       = CUTSCENE_SKIP_DISABLED;
        D_shelter_b1_sterilization_room_8018C344.startSound      = CUTSCENE_START_SOUND;
        D_shelter_b1_sterilization_room_8018C344.endSound        = CUTSCENE_END_SOUND;
        D_shelter_b1_sterilization_room_8018C344.sceneSound      = CUTSCENE_SCENE_SOUND;
        D_shelter_b1_sterilization_room_8018C344.afterSceneSound = CUTSCENE_AFTER_SCENE_SOUND;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, CUTSCENE_FOLLOWUP_CAP_COMMAND, &D_shelter_b1_sterilization_room_8018C344);
    }
    if (command == COMMAND_CAP_21) {
        capRunCommandWithTransition(command);
    }
    if (command == SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_DIALOGUE) {
        taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DIALOGUE, SHELTER_B1_STERILIZATION_ROOM_DIALOGUE_PRIMARY, 0);
    }
    if (command == SHELTER_B1_STERILIZATION_ROOM_COMMAND_ALTERNATE_TRAP_DIALOGUE) {
        taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DIALOGUE, SHELTER_B1_STERILIZATION_ROOM_DIALOGUE_ALTERNATE, 0);
    }
    if (command == COMMAND_TRAP_EXCHANGE) {
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_TRIGGERED) == TRAP_TRIGGERED && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == TRAP_RUNNING) {
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F) == SCENE_NOT_STARTED) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                evsStartScriptWithSkip(D_shelter_b1_sterilization_room_80188ED4, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_shelter_b1_sterilization_room_80188FDC);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F, SCENE_FIRST_STARTED);
            } else {
                taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE, SCENE_REPEAT_TRAP_EXCHANGE, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14F, SCENE_REPEAT_STARTED);
            }
        } else {
            capRunCommandWithTransition(command);
        }
    }
    if (command == COMMAND_PRESENTATION_12 || command == COMMAND_PRESENTATION_13) {
        if (gGameSession->location.loc.room == PRESENTATION_ROOM) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_shelter_b1_sterilization_room_80184E70, 0, command, 0);
        }
    }
    return 0;
}

/// Runs an in-place CAP command between the player's entry and exit presentation clips.
///
/// Starts in state 0 with player control already held. `spawnArg1.value` is a
/// valid command in loaded data-resource ordinal 1 (callers use 12 or 13);
/// its texture must be at VRAM (704,256). States 1/6 allow the deferred clips
/// to start before states 2/7 poll animation completion. State 4 waits for CAP;
/// state 8 resumes player control, restores the default CAP resource and kills
/// the task. `spawnArg2` is unused; keep player, CAP and overlay resources live.
static void _shelterB1SterilizationRoomScriptedPresentationTask(Task* task)
{
    enum {
        PRESENTATION_ENTER             = 0,
        PRESENTATION_ENTER_DELAY       = 1,
        PRESENTATION_WAIT_ENTER        = 2,
        PRESENTATION_PLAY_CAP          = 3,
        PRESENTATION_WAIT_CAP          = 4,
        PRESENTATION_LEAVE             = 5,
        PRESENTATION_LEAVE_DELAY       = 6,
        PRESENTATION_WAIT_LEAVE        = 7,
        PRESENTATION_FINISH            = 8,
        PLAYER_PRESENTATION_ENTER_CLIP = 0,
        PLAYER_PRESENTATION_LEAVE_CLIP = 1,
        SCENE_FILE_ORDINAL             = 1,
        SCENE_TEXTURE_VRAM_X           = 704,
        SCENE_TEXTURE_VRAM_Y           = 256,
    };
    switch (task->state) {
        case PRESENTATION_ENTER:
            Gp_CapFile = NULL;
            capSelectLoadedFile(SCENE_FILE_ORDINAL);
            capSetTexturePage(SCENE_TEXTURE_VRAM_X, SCENE_TEXTURE_VRAM_Y);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, PLAYER_PRESENTATION_ENTER_CLIP, 0);
            task->state++;
            break;
        // Presentation playback begins on a later player tick; allow it to start.
        case PRESENTATION_ENTER_DELAY:
            task->state++;
            break;
        case PRESENTATION_WAIT_ENTER:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                task->state++;
            }
            break;
        case PRESENTATION_PLAY_CAP:
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case PRESENTATION_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case PRESENTATION_LEAVE:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, PLAYER_PRESENTATION_LEAVE_CLIP, 0);
            task->state++;
            break;
        case PRESENTATION_LEAVE_DELAY:
            task->state++;
            break;
        case PRESENTATION_WAIT_LEAVE:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                task->state++;
            }
            break;
        case PRESENTATION_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Initializes the event actor's collision obstacle, raising inactive geometry out of reach.
///
/// Uses placed actor 0, or the live player's model root if that actor is absent.
/// Variant 5 with event progress 1 keeps the obstacle at the actor; other cases
/// add 10000 game units along room Y. The selected task must have a live TMD root.
/// The translation array and reserved leading room-grid entries persist with
/// the overlay; only initialization invokes this rebuild. The argument is unused.
static void _shelterB1SterilizationRoomInitializeActorObstacle(s32 unused)
{
    enum { SHELTER_B1_STERILIZATION_ROOM_INACTIVE_OBSTACLE_HEIGHT = 10000 };
    Task* placedActor   = sceneFindPlacedActor(SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_PLACEMENT);
    Task* modelActor    = placedActor;
    s32   usePlayerRoot = (placedActor == NULL);

    if (usePlayerRoot) {
        modelActor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    if (placedActor != NULL) {
        if (gGameSession->location.loc.variant == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_VARIANT && gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_ACTIVE) {
            D_shelter_b1_sterilization_room_80184E80[1] = 0;
        } else {
            D_shelter_b1_sterilization_room_80184E80[1] = SHELTER_B1_STERILIZATION_ROOM_INACTIVE_OBSTACLE_HEIGHT;
        }
    } else {
        D_shelter_b1_sterilization_room_80184E80[1] = SHELTER_B1_STERILIZATION_ROOM_INACTIVE_OBSTACLE_HEIGHT;
    }
    _shelterB1SterilizationRoomRebuildActorObstacle(modelActor->extra.tmd->coords, D_shelter_b1_sterilization_room_80184E80);
}

/// Refuses every room key-item request without consuming the selected item.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are unused. The reply
/// selects the item menu's refused-item presentation.
static s32 _shelterB1SterilizationRoomRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a requested room transition using Mine/Shelter event progress.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows two live eight-byte records for this call; `reply` must be writable
/// and may alias `request`. Copies the entire request, then adjusts its room
/// selector through `mapShelterRoomVariantResolve`. Queries preserve the copied
/// destination. Returns 1 to accept the transition; retains neither pointer.
static s32 _shelterB1SterilizationRoomResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B1_STERILIZATION_ROOM_TRANSITION_ACCEPTED = 1 };
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B1_STERILIZATION_ROOM_TRANSITION_ACCEPTED;
}

/// Starts the room's sound script 22 for CAP sound cue 99.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue values do nothing. The task,
/// message ID and second payload are unused. Always returns zero.
static s32 _shelterB1SterilizationRoomSoundMessage(Task* task, s32 messageId, s32 soundCue, s32 unused)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_SOUND_CUE_99  = 99,
        SHELTER_B1_STERILIZATION_ROOM_CUE_99_SCRIPT = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 22),
    };
    if (soundCue == SHELTER_B1_STERILIZATION_ROOM_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SHELTER_B1_STERILIZATION_ROOM_CUE_99_SCRIPT, 0, 0);
    }
    return 0;
}

/// Updates the event actor's view-dependent visibility while room variant 5 is active.
///
/// The room task remains in this state; `task` is unused.
static void _shelterB1SterilizationRoomUpdateState(Task* task)
{
    if (gGameSession->location.loc.variant == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_VARIANT) {
        _shelterB1SterilizationRoomUpdateEventActorVisibility();
    }
}

/// Shows event actor 0 only in views 2 and 3 while its event is active and scripts are idle.
///
/// Other event progress or an active script preserves the actor's draw mode.
/// The scene must be live; an absent placed actor is ignored by the scene API.
static void _shelterB1SterilizationRoomUpdateEventActorVisibility(void)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_VIEW_2 = 2,
        SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_VIEW_3 = 3,
        SHELTER_B1_STERILIZATION_ROOM_ACTOR_HIDDEN       = 0,
        SHELTER_B1_STERILIZATION_ROOM_ACTOR_VISIBLE      = 1,
    };
    s32 view;

    view = gGameSession->location.loc.view;
    if ((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_ACTIVE) && (gGameSession->eventState == 0)) {
        if (view == SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_VIEW_2 || view == SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_VIEW_3) {
            sceneSetPlacedActorDrawMode(SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_PLACEMENT, SHELTER_B1_STERILIZATION_ROOM_ACTOR_VISIBLE);
        } else {
            sceneSetPlacedActorDrawMode(SHELTER_B1_STERILIZATION_ROOM_EVENT_ACTOR_PLACEMENT, SHELTER_B1_STERILIZATION_ROOM_ACTOR_HIDDEN);
        }
    }
}

void shelterB1SterilizationRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b1_sterilization_room_8017D6A4;
    states.funcs[task->state](task);
}

/// Rebuilds the event actor's four-face obstacle in the leading room-grid entries.
///
/// `modelRoot->coord` must map obstacle-local coordinates into room axes; parent
/// and view transforms are not composed. Rotation and normals are Q12 (4096
/// equals one); vertices and translation use whole game coordinate units.
/// `roomOffset` is NULL or three borrowed signed-halfword XYZ offsets, added to
/// the matrix's translation before the GTE transforms eight vertices.
/// Source and destination have disjoint, word-aligned vector pools. Only four
/// normals/faces and eight vertices change; cell lists and later geometry stay
/// intact. XYZ normal writes preserve their pads; the SDK vertex transform also
/// writes each vertex's pad. Requires a live root, changes GTE state and discards
/// transform flags. Neither input is retained.
static void _shelterB1SterilizationRoomRebuildActorObstacle(const GfxCoord* modelRoot, const s16 roomOffset[3])
{
    MATRIX                    modelToRoom;
    long                      transformFlags;
    s32                       elementIndex;
    SVECTOR*                  destinationVector;
    SVECTOR*                  sourceVector;
    WorldCollisionGrid*       roomGrid       = &D_shelter_b1_sterilization_room_80189E44;
    const WorldCollisionGrid* obstacleSource = &D_shelter_b1_sterilization_room_80184F28;

    /// Restores the reserved obstacle's XYZ vectors and complete faces.
    ///
    /// Captures `roomGrid`, `obstacleSource`, `elementIndex` and the two counts
    /// above. Pools must be disjoint with four normals/faces and eight vertices.
    /// Preserves vector pads and later geometry; leaves the index at eight.
    /// This argument-free compound statement is undefined after its single use.
#define SHELTER_B1_STERILIZATION_ROOM_RESTORE_OBSTACLE_GEOMETRY()                            \
    {                                                                                        \
        elementIndex = 0;                                                                    \
        do {                                                                                 \
            roomGrid->normals[elementIndex].vx = obstacleSource->normals[elementIndex].vx;   \
            roomGrid->normals[elementIndex].vy = obstacleSource->normals[elementIndex].vy;   \
            roomGrid->normals[elementIndex].vz = obstacleSource->normals[elementIndex].vz;   \
            roomGrid->faces[elementIndex]      = obstacleSource->faces[elementIndex];        \
            elementIndex++;                                                                  \
        } while (elementIndex < SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_FACE_COUNT);          \
        elementIndex = 0;                                                                    \
        do {                                                                                 \
            roomGrid->vertices[elementIndex].vx = obstacleSource->vertices[elementIndex].vx; \
            roomGrid->vertices[elementIndex].vy = obstacleSource->vertices[elementIndex].vy; \
            roomGrid->vertices[elementIndex].vz = obstacleSource->vertices[elementIndex].vz; \
            elementIndex++;                                                                  \
        } while (elementIndex < SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_VERTEX_COUNT);        \
    }

    SHELTER_B1_STERILIZATION_ROOM_RESTORE_OBSTACLE_GEOMETRY();
#undef SHELTER_B1_STERILIZATION_ROOM_RESTORE_OBSTACLE_GEOMETRY

    modelToRoom = modelRoot->coord;

    if (roomOffset != NULL) {
        modelToRoom.t[0] += roomOffset[0];
        modelToRoom.t[1] += roomOffset[1];
        modelToRoom.t[2] += roomOffset[2];
    }

    // Translation affects the vertices; normals carry rotation only.
    destinationVector = roomGrid->normals;
    sourceVector      = obstacleSource->normals;
    elementIndex      = 0;
    do {
        gte_SetRotMatrix(&modelToRoom);
        gte_ldv0(sourceVector);
        sourceVector++;
        gte_rtv0();
        gte_stsv(destinationVector);
        destinationVector++;
        elementIndex++;
    } while (elementIndex < SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_FACE_COUNT);

    gte_SetRotMatrix(&modelToRoom);
    gte_SetTransMatrix(&modelToRoom);
    destinationVector = roomGrid->vertices;
    sourceVector      = obstacleSource->vertices;
    for (elementIndex = 0; elementIndex < SHELTER_B1_STERILIZATION_ROOM_OBSTACLE_VERTEX_COUNT; elementIndex++) {
        RotTransSV(sourceVector++, destinationVector++, &transformFlags);
    }
}

/// Captures the current 320x240 draw buffer in two VRAM strips for the room crossfade.
///
/// Stores 192 columns at (832, 0) and 128 at (384, 256), using source row 0 or
/// 272 for draw buffer 0 or 1. OT head insertion forces destination mask bits
/// on during the copies, then disables forced mask writes afterwards. Requires
/// the source frame drawn before OT slot 8 and free packet storage for two
/// `DR_STP`s and two `DR_MOVE`s (72 bytes), retained until GPU completion.
/// Advances the task's state after queuing; capture completes asynchronously.
static void _shelterB1SterilizationRoomCaptureCrossfadeState(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_LOWER_BUFFER_Y = 272,
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_LEFT_VRAM_X    = 832,
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_RIGHT_VRAM_X   = 384,
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_RIGHT_VRAM_Y   = 256,
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_MASK_OFF       = 0,
        SHELTER_B1_STERILIZATION_ROOM_CAPTURE_MASK_ON        = 1,
    };
    RECT     rect;
    DR_STP*  maskPacket;
    DR_MOVE* copyPacket;
    s16      sourceX;
    s16      sourceY;

    if (gDisplayState.drawBuffer == 0) {
        sourceX = 0;
        sourceY = 0;
    } else {
        sourceX = 0;
        sourceY = SHELTER_B1_STERILIZATION_ROOM_CAPTURE_LOWER_BUFFER_Y;
    }

    // Head insertion runs the last mask packet first and this one after both copies.
    maskPacket     = gGpuPrimCursor;
    gGpuPrimCursor = maskPacket + 1;
    SetDrawStp(maskPacket, SHELTER_B1_STERILIZATION_ROOM_CAPTURE_MASK_OFF);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskPacket);

    copyPacket     = gGpuPrimCursor;
    gGpuPrimCursor = copyPacket + 1;
    rect.x         = sourceX;
    rect.y         = sourceY;
    rect.w         = CROSSFADE_BACKDROP_LEFT_WIDTH;
    rect.h         = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyPacket, &rect, SHELTER_B1_STERILIZATION_ROOM_CAPTURE_LEFT_VRAM_X, 0);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyPacket);

    copyPacket     = gGpuPrimCursor;
    gGpuPrimCursor = copyPacket + 1;
    rect.x         = sourceX + CROSSFADE_BACKDROP_LEFT_WIDTH;
    rect.y         = sourceY;
    rect.w         = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    rect.h         = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyPacket, &rect, SHELTER_B1_STERILIZATION_ROOM_CAPTURE_RIGHT_VRAM_X, SHELTER_B1_STERILIZATION_ROOM_CAPTURE_RIGHT_VRAM_Y);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyPacket);

    maskPacket     = gGpuPrimCursor;
    gGpuPrimCursor = maskPacket + 1;
    SetDrawStp(maskPacket, SHELTER_B1_STERILIZATION_ROOM_CAPTURE_MASK_ON);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskPacket);

    task->state++;
}

#include "../../shared/backdrop_crossfade_live.inc.c"

/// Queues an additive redraw of the captured backdrop for the room crossfade.
///
/// Samples 192x240 pixels at VRAM (832, 0) and 128x240 at (384, 256), placing
/// them over the centered 320x240 frame. `shade` is RGB modulation (0 black,
/// 0x80 unchanged); only its low byte is stored. Requires a completed capture
/// and room for two `SPRT`s and two `DR_TPAGE`s in the current frame arena,
/// borrowed by the GPU until completion. No capacity checks are made.
static void _crossfadeDrawBackdrop(s32 shade)
{
    enum {
        CROSSFADE_STERILIZATION_LEFT_VRAM_X  = 832,
        CROSSFADE_STERILIZATION_RIGHT_VRAM_X = 384,
        CROSSFADE_STERILIZATION_RIGHT_VRAM_Y = 256,
    };
    SPRT* sprite;

    /// Reserves and initializes one semitransparent, greyscale backdrop sprite.
    ///
    /// `spritePacket` must be a writable SPRT pointer local and `shadeValue` a
    /// side-effect-free value, read three times. Uses the current frame arena;
    /// the caller fills geometry and linkage and retains storage for the GPU.
#define CROSSFADE_ALLOCATE_BACKDROP_SPRITE(spritePacket, shadeValue) \
    {                                                                \
        (spritePacket) = gGpuPrimCursor;                             \
        gGpuPrimCursor = (spritePacket) + 1;                         \
        setSprt(spritePacket);                                       \
        setSemiTrans(spritePacket, 1);                               \
        (spritePacket)->r0 = (shadeValue);                           \
        (spritePacket)->g0 = (shadeValue);                           \
        (spritePacket)->b0 = (shadeValue);                           \
    }

    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = -CROSSFADE_BACKDROP_WIDTH / 2;
    sprite->y0   = -CROSSFADE_BACKDROP_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = CROSSFADE_BACKDROP_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(CROSSFADE_STERILIZATION_LEFT_VRAM_X, 0);

    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = CROSSFADE_BACKDROP_LEFT_WIDTH - CROSSFADE_BACKDROP_WIDTH / 2;
    sprite->y0   = -CROSSFADE_BACKDROP_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = CROSSFADE_BACKDROP_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(CROSSFADE_STERILIZATION_RIGHT_VRAM_X, CROSSFADE_STERILIZATION_RIGHT_VRAM_Y);
#undef CROSSFADE_ALLOCATE_BACKDROP_SPRITE
}

/// Moves the player through an internal door with a 30-tick fade on each side.
///
/// Starts at state 0 with a zeroed signed-halfword `killCountdown` and a door
/// index 0..7 in `spawnArg1.value` (room action minus 3). Updates the live save
/// and session view together and synchronously places the player at the paired
/// destination. Holds scripted control until the fade-in finishes, then releases
/// control and the task. Requires the room's destination tables and live player.
static void _shelterB1SterilizationRoomDoorPassageTask(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_DOOR_HOLD_PLAYER    = 0,
        SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_OUT       = 1,
        SHELTER_B1_STERILIZATION_ROOM_DOOR_PLACE_PLAYER   = 2,
        SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_IN        = 3,
        SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_TICKS     = 30,
        SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE = 0xFF,
    };
    u8 fadeShade;

    switch (task->state) {
        case SHELTER_B1_STERILIZATION_ROOM_DOOR_HOLD_PLAYER:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_DOOR_OPEN, 0, 0);
            task->state++;
            break;
        case SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_OUT:
            if (++task->killCountdown >= SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_TICKS) {
                task->state++;
            }
            fadeShade = (task->killCountdown * SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE / SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_TICKS) & SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE;
            fadeDrawOverlay(fadeShade, fadeShade, fadeShade, GPU_BLEND_SUBTRACT);
            break;
        case SHELTER_B1_STERILIZATION_ROOM_DOOR_PLACE_PLAYER:
            // Switch the view and placement while the screen is fully dark.
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->location.loc.view                            = D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].view;
            gGameSession->viewDirty                                    = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE,
                                          &D_shelter_b1_sterilization_room_80188668[D_shelter_b1_sterilization_room_80188728[task->spawnArg1.value].placementIndex],
                                          0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_DOOR_CLOSE, 0, 0);
            fadeDrawOverlay(SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE, SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE, SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE, GPU_BLEND_SUBTRACT);
            task->state++;
            break;
        case SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_IN:
            if (--task->killCountdown <= 0) {
                task->state++;
            }
            fadeShade = (task->killCountdown * SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE / SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_TICKS) & SHELTER_B1_STERILIZATION_ROOM_DOOR_FADE_MAX_SHADE;
            fadeDrawOverlay(fadeShade, fadeShade, fadeShade, GPU_BLEND_SUBTRACT);
            break;
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
    }
}

/// Applies the sterilization trap's periodic damage and player reaction while it is running.
///
/// Requires state 0 and zeroed `killCountdown`/spawn arguments. State 1 counts
/// only ticks with idle scripts and running actors: resumes player control at
/// tick 73, damages at 120 and starts the survivor reaction at 121, then resets
/// the counter. A fatal hit leaves the counter running. Pauses retain the counter
/// and stop the hurt sound once (`spawnArg1.value`: 0 running, 1 stopped).
/// Trap completion stops the sound and releases the task. Uses the live player's
/// model and borrows the room's animation bank; callback-owned counters remain
/// signed halfwords. Writes restart code 2, whose wider role is unproven.
static void _shelterB1SterilizationRoomTrapDamageTask(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_TRAP_INITIALIZE    = 0,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_ACTIVE        = 1,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_RESTART_CODE  = 2,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_RESUME_TICK   = 73,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_DAMAGE_TICK   = 120,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_REACTION_TICK = 121,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_SOUND_RUNNING = 0,
        SHELTER_B1_STERILIZATION_ROOM_TRAP_SOUND_STOPPED = 1,
    };
    Task*     playerTask;
    GfxCoord* playerRoot;
    s32       audioPan;

    switch (task->state) {
        case SHELTER_B1_STERILIZATION_ROOM_TRAP_INITIALIZE:
            gGameSession->restartMode = SHELTER_B1_STERILIZATION_ROOM_TRAP_RESTART_CODE;
            task->state++;
            return;
        case SHELTER_B1_STERILIZATION_ROOM_TRAP_ACTIVE:
            if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED) == 0) {
                if (gGameSession->eventState == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    task->killCountdown++;
                    if (task->killCountdown == SHELTER_B1_STERILIZATION_ROOM_TRAP_DAMAGE_TICK) {
                        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_shelter_b1_sterilization_room_80188738, 0), 0);
                    } else if (task->killCountdown >= SHELTER_B1_STERILIZATION_ROOM_TRAP_REACTION_TICK) {
                        // Delay the reaction until the tick after damage; a fatal hit skips it.
                        if (gPlayerStatus.hp > 0) {
                            playerRoot = playerTask->extra.tmd->coords;
                            TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_shelter_b1_sterilization_room_80188590, 0);
                            playerActorWriteWeaponAnimationBankIndex(&D_shelter_b1_sterilization_room_80188624.source.index);
                            TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_PLAY, &D_shelter_b1_sterilization_room_80188624, 0);
                            audioPan = (s8)worldCoordGetOriginAudioPan(playerRoot);
                            sndEvtRequestScriptStart(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, audioPan, (s8)worldCoordGetOriginAudioDepth(playerRoot));
                            task->killCountdown = 0;
                        }
                        Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                    } else if (task->killCountdown == SHELTER_B1_STERILIZATION_ROOM_TRAP_RESUME_TICK) {
                        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                    }
                    task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SOUND_RUNNING;
                    return;
                }
                if (task->spawnArg1.value == SHELTER_B1_STERILIZATION_ROOM_TRAP_SOUND_RUNNING) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SOUND_STOPPED;
                }
                return;
            }
            sndEvtRequestScriptStop(SOUND_SHELTER_B1_STERILIZATION_PLAYER_HURT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        default:
            taskKill(task);
            break;
    }
}

void shelterB1SterilizationRoomSpawnTaskOnce(s32 descriptorIndex)
{
    if (!((D_shelter_b1_sterilization_room_8018C340 >> descriptorIndex) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << descriptorIndex;
        taskSpawnFromTable(D_shelter_b1_sterilization_room_80188504, descriptorIndex, 0, 0);
    }
}

/// The four states of the backdrop task run by
/// `_shelterB1SterilizationRoomCrossfadeTask`: copy the frame buffer into
/// the backdrop, wait for the view, fade the copy out and kill.
static const TaskFuncTable4 D_shelter_b1_sterilization_room_8017D700 = {
    {
        _shelterB1SterilizationRoomCaptureCrossfadeState,
        _shelterB1SterilizationRoomWaitCrossfadeViewState,
        _crossfadeOutState,
        taskKill,
    },
};

/// Captures the old frame, waits for the replacement view and crossfades to it over 16 ticks.
///
/// Starts at state 0; valid dispatch states are 0 capture, 1 wait, 2 fade and
/// 3 teardown. `killCountdown` becomes the saved frame's RGB modulation shade
/// (128 unchanged), reduced by eight each fade tick. Spawn arguments are unused.
/// Requires the overlay's reserved VRAM strips and packet arena; the queued
/// capture must finish before the next callback samples the saved backdrop.
static void _shelterB1SterilizationRoomCrossfadeTask(Task* task)
{
    TaskFuncTable4 states;

    states = D_shelter_b1_sterilization_room_8017D700;
    states.funcs[task->state](task);
}

/// Holds the captured frame on screen until the replacement view is ready.
///
/// Queues the saved image at unity and the live frame at zero on every tick.
/// A nonzero `viewReady` initializes `killCountdown` to the unity shade and
/// advances into fading. Requires a completed capture and packet storage for
/// four sprites and four texture-page commands (112 bytes) until GPU completion.
static void _shelterB1SterilizationRoomWaitCrossfadeViewState(Task* task)
{
    _crossfadeDrawBackdrop(CROSSFADE_SHADE_UNITY);
    _crossfadeDrawLive(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = CROSSFADE_SHADE_UNITY;
        task->state++;
    }
}

#include "../../shared/backdrop_crossfade_out.inc.c"

#include "../../shared/backdrop_crossfade_tpage.inc.c"
