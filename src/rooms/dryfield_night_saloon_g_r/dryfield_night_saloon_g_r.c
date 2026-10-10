#include "rooms/dryfield_night_saloon_g_r.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8018055c.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"
#include "../../shared/jukebox.h"
#include "../../shared/room_variants.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The view slot the room was on when its cutscene took over, restored when the cutscene ends.
extern RoomSavedViewStorage D_dryfield_night_saloon_g_r_80188FA4;

extern TaskMessageEntry D_dryfield_night_saloon_g_r_8017F918[];
extern TaskDesc         D_dryfield_night_saloon_g_r_8017F940[];

/// Cutscene script blobs handed to `evsStartScript` / `evsStartScriptWithSkip`.
extern EvsCommand D_dryfield_night_saloon_g_r_801848DC[];
extern EvsCommand D_dryfield_night_saloon_g_r_80184B34[];
extern EvsCommand D_dryfield_night_saloon_g_r_80184D2C[];
extern EvsCommand D_dryfield_night_saloon_g_r_80183C94[];
extern EvsCommand D_dryfield_night_saloon_g_r_801847A4[];

/// Descriptor of the jukebox menu panel, whose task is
/// `_dryfieldNightSaloonGRJukeboxMenuTask`.
extern UiObjectDesc gJukeboxPanelDesc;

/// Descriptor of the jukebox task `_jukeboxHostTask`.
extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

static void _dryfieldNightSaloonGRInitializeRoom(Task* task);
static void _dryfieldNightSaloonGRIdleState(Task* task);
static s32  _dryfieldNightSaloonGRQueueJukebox(s32 unusedArg);
static void _dryfieldNightSaloonGRDrawTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale);

static void _dryfieldNightSaloonGRJukeboxMenuTask(Task* task);

static WorldCoordRoomLights _gDryfieldNightSaloonGRRoomLights;

static WorldCollisionGrid     _gDryfieldNightSaloonGRCollisionGrid;
extern WorldCollisionOccluder D_dryfield_night_saloon_g_r_80188E18[2];
static WorldCollisionTrigger  _gDryfieldNightSaloonGRViewBoundaryTriggers[16];
static WorldCollisionTrigger  _gDryfieldNightSaloonGRActionTriggers[21];

extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_8018397C;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183990;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_801839B8;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_801839CC;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_801839F4;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183A08;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183A1C;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183A6C;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183A94;
extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183AA8;
extern AnimationBankCopyRequest D_dryfield_night_saloon_g_r_8018394C;
extern GameActorMoveAnim        D_dryfield_night_saloon_g_r_80183B34;
extern ActorTransform           D_dryfield_night_saloon_g_r_80183ABC;
extern ActorTransform           D_dryfield_night_saloon_g_r_80183AD4;
extern ActorTransform           D_dryfield_night_saloon_g_r_80183AEC;
extern ActorTransform           D_dryfield_night_saloon_g_r_80183B04;
extern ActorTransform           D_dryfield_night_saloon_g_r_80183B1C;
static const char               _gDryfieldNightSaloonGRJukeboxRow3HeavenSentKillerLabel[22];
static const char               _gDryfieldNightSaloonGRJukeboxRow2EagerForBloodLabel[19];
static const char               _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel[14];
static const char               _gDryfieldNightSaloonGRJukeboxRow3CrawlingWasteEmperorLabel[26];
static const char               _gDryfieldNightSaloonGRJukeboxRow2PickUpTheGauntletLabel[24];
static const char               _gDryfieldNightSaloonGRJukeboxRow3HuntersMoonLabel[17];
static const char               _gDryfieldNightSaloonGRJukeboxRow2QuadrumanousLeaderLabel[23];
static const char               _gDryfieldNightSaloonGRJukeboxRow3GenicReactorLabel[17];
static const char               _gDryfieldNightSaloonGRJukeboxRow2RebelForcesLabel[16];
static const char               _gDryfieldNightSaloonGRJukeboxRow1YellowRainLabel[15];
static const char               _gDryfieldNightSaloonGRJukeboxRow3PickUpTheGauntletLabel[24];
static const char               _gDryfieldNightSaloonGRJukeboxRow2AmbushLabel[11];
static const char               _gDryfieldNightSaloonGRJukeboxRow4DarkVoiceOfTheHeartLabel[27];
static const char               _gDryfieldNightSaloonGRJukeboxRow3RequiemLabel[11];
static const char               _gDryfieldNightSaloonGRJukeboxRow2RockAndFireLabel[17];
static const char               _gDryfieldNightSaloonGRJukeboxRow1GhostTownLabel[14];
static const char               _gDryfieldNightSaloonGRJukeboxRow4SnooperLabel[11];
static const char               _gDryfieldNightSaloonGRJukeboxRow3WildHuntLabel[13];
static const char               _gDryfieldNightSaloonGRJukeboxRow2LightningOperationLabel[23];
static const char               _gDryfieldNightSaloonGRJukeboxRow1LaMazeLabel[13];
static const char               _gDryfieldNightSaloonGRJukeboxRow4GenicReactorLabel[17];
static const char               _gDryfieldNightSaloonGRJukeboxRow3MentalAgonyLabel[16];
static const char               _gDryfieldNightSaloonGRJukeboxRow2WishwashLabel[12];
static const char               _gDryfieldNightSaloonGRJukeboxRow1CurseYourFateLabel[20];
static const char               _gDryfieldNightSaloonGRJukeboxRow4KillingFieldLabel[17];
static const char               _gDryfieldNightSaloonGRJukeboxRow3FoolsParadiseLabel[19];
static const char               _gDryfieldNightSaloonGRJukeboxRow2GazingIntoTheVoidLabel[24];
static const char               _gDryfieldNightSaloonGRJukeboxRow1ManMadeNatureLabel[19];
static const char               _gDryfieldNightSaloonGRJukeboxRow4LazingAwayTheMorningLabel[27];
static const char               _gDryfieldNightSaloonGRJukeboxRow3OutOfPhase2Label[18];
static const char               _gDryfieldNightSaloonGRJukeboxRow2TheVagrantsLabel[16];
static const char               _gDryfieldNightSaloonGRJukeboxRow1TowerRendezvousLabel[20];
static void                     _dryfieldNightSaloonGRSetRoom(u8 roomId);

static s32  _dryfieldNightSaloonGRRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32  _dryfieldNightSaloonGRCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32  _dryfieldNightSaloonGRRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _dryfieldNightSaloonGRJukeboxSceneTask(Task* task);

enum {
    DRYFIELD_NIGHT_SALOON_G_R_MESSAGE_USE_KEY_ITEM = 0x13F1,
    DRYFIELD_NIGHT_SALOON_G_R_ROOM_AFTER_CUTSCENE  = 2,
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_saloon_g_r_8017F918[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantSaloonMsg },
    { DRYFIELD_NIGHT_SALOON_G_R_MESSAGE_USE_KEY_ITEM, _dryfieldNightSaloonGRRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightSaloonGRRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightSaloonGRCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_saloon_g_r_8017F940[1] = {
    { { { TASK_BODY_NONE, 192 } }, _dryfieldNightSaloonGRJukeboxSceneTask, { .value = 0 } },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation02668Bank1[6] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation02668Bank4[46] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation02668Records[109] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation02668Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02668_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation02668 = {
    _gDryfieldNightSaloonGRAnimation02668Records,
    _gDryfieldNightSaloonGRAnimation02668Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation02668Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation02668Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation02A3CBank1[8] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation02A3CBank4[84] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation02A3CRecords[117] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation02A3CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02A3C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation02A3C = {
    _gDryfieldNightSaloonGRAnimation02A3CRecords,
    _gDryfieldNightSaloonGRAnimation02A3CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation02A3CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation02A3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation02E2CBank1[7] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation02E2CBank4[62] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation02E2CRecords[149] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation02E2CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_02E2C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation02E2C = {
    _gDryfieldNightSaloonGRAnimation02E2CRecords,
    _gDryfieldNightSaloonGRAnimation02E2CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation02E2CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation02E2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation03198Bank1[7] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation03198Bank4[74] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation03198Records[104] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation03198Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03198_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation03198 = {
    _gDryfieldNightSaloonGRAnimation03198Records,
    _gDryfieldNightSaloonGRAnimation03198Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation03198Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation03198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation036B8Bank1[10] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation036B8Bank4[100] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation036B8Records[178] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation036B8Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_036B8_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation036B8 = {
    _gDryfieldNightSaloonGRAnimation036B8Records,
    _gDryfieldNightSaloonGRAnimation036B8Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation036B8Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation036B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation03998Bank1[5] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation03998Bank4[59] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation03998Records[90] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation03998Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03998_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation03998 = {
    _gDryfieldNightSaloonGRAnimation03998Records,
    _gDryfieldNightSaloonGRAnimation03998Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation03998Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation03998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation03E80Bank1[6] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation03E80Bank4[110] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation03E80Records[166] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation03E80Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_03E80_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation03E80 = {
    _gDryfieldNightSaloonGRAnimation03E80Records,
    _gDryfieldNightSaloonGRAnimation03E80Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation03E80Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation03E80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation04238Bank1[3] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation04238Bank4[81] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation04238Records[128] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation04238Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04238_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation04238 = {
    _gDryfieldNightSaloonGRAnimation04238Records,
    _gDryfieldNightSaloonGRAnimation04238Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation04238Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation04238Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation04430Bank1[3] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation04430Bank4[34] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation04430Records[63] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation04430Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04430_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation04430 = {
    _gDryfieldNightSaloonGRAnimation04430Records,
    _gDryfieldNightSaloonGRAnimation04430Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation04430Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation04430Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation04784Bank1[5] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation04784Bank4[58] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation04784Records[120] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation04784Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04784_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation04784 = {
    _gDryfieldNightSaloonGRAnimation04784Records,
    _gDryfieldNightSaloonGRAnimation04784Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation04784Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation04784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation0497CBank1[3] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation0497CBank4[34] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation0497CRecords[63] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation0497CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0497C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation0497C = {
    _gDryfieldNightSaloonGRAnimation0497CRecords,
    _gDryfieldNightSaloonGRAnimation0497CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation0497CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation0497CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation04C24Bank1[4] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation04C24Bank4[46] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation04C24Records[92] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation04C24Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04C24_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation04C24 = {
    _gDryfieldNightSaloonGRAnimation04C24Records,
    _gDryfieldNightSaloonGRAnimation04C24Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation04C24Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation04C24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation04F58Bank1[6] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation04F58Bank4[68] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation04F58Records[99] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation04F58Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_04F58_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation04F58 = {
    _gDryfieldNightSaloonGRAnimation04F58Records,
    _gDryfieldNightSaloonGRAnimation04F58Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation04F58Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation04F58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation053C0Bank1[10] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation053C0Bank4[96] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation053C0Records[136] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation053C0Indices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_053C0_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation053C0 = {
    _gDryfieldNightSaloonGRAnimation053C0Records,
    _gDryfieldNightSaloonGRAnimation053C0Indices,
    { NULL, _gDryfieldNightSaloonGRAnimation053C0Bank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation053C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation0570CBank1[6] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation0570CBank4[71] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation0570CRecords[102] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation0570CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0570C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation0570C = {
    _gDryfieldNightSaloonGRAnimation0570CRecords,
    _gDryfieldNightSaloonGRAnimation0570CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation0570CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation0570CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation05B7CBank1[5] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation05B7CBank4[99] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation05B7CRecords[150] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation05B7CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_05B7C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation05B7C = {
    _gDryfieldNightSaloonGRAnimation05B7CRecords,
    _gDryfieldNightSaloonGRAnimation05B7CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation05B7CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation05B7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightSaloonGRAnimation0631CBank1[13] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightSaloonGRAnimation0631CBank4[179] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_bank4.inc"
};

static AnimationRecord _gDryfieldNightSaloonGRAnimation0631CRecords[250] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_records.inc"
};

static u16 _gDryfieldNightSaloonGRAnimation0631CIndices[20] = {
#include "assets/dryfield_night_saloon_g_r_animation_0631C_indices.inc"
};

static AnimationSet _gDryfieldNightSaloonGRAnimation0631C = {
    _gDryfieldNightSaloonGRAnimation0631CRecords,
    _gDryfieldNightSaloonGRAnimation0631CIndices,
    { NULL, _gDryfieldNightSaloonGRAnimation0631CBank1, NULL, NULL, _gDryfieldNightSaloonGRAnimation0631CBank4, NULL, NULL, NULL },
};

AnimationSet* D_dryfield_night_saloon_g_r_80183904[18] = {
    NULL,
    &_gDryfieldNightSaloonGRAnimation02668,
    &_gDryfieldNightSaloonGRAnimation04C24,
    &_gDryfieldNightSaloonGRAnimation04F58,
    &_gDryfieldNightSaloonGRAnimation053C0,
    &_gDryfieldNightSaloonGRAnimation02A3C,
    &_gDryfieldNightSaloonGRAnimation02E2C,
    &_gDryfieldNightSaloonGRAnimation03198,
    &_gDryfieldNightSaloonGRAnimation03E80,
    &_gDryfieldNightSaloonGRAnimation036B8,
    &_gDryfieldNightSaloonGRAnimation03998,
    &_gDryfieldNightSaloonGRAnimation04430,
    &_gDryfieldNightSaloonGRAnimation04784,
    &_gDryfieldNightSaloonGRAnimation0497C,
    &_gDryfieldNightSaloonGRAnimation05B7C,
    &_gDryfieldNightSaloonGRAnimation0631C,
    &_gDryfieldNightSaloonGRAnimation0570C,
    &_gDryfieldNightSaloonGRAnimation04238,
};

AnimationBankCopyRequest D_dryfield_night_saloon_g_r_8018394C = { { .sets = D_dryfield_night_saloon_g_r_80183904 }, ARRAY_SIZE(D_dryfield_night_saloon_g_r_80183904) };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183954 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    DRYFIELD_NIGHT_SALOON_G_R_PLAYER_DIALOGUE_SET          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    DRYFIELD_NIGHT_SALOON_G_R_PLAYER_DIALOGUE_BLEND_FRAMES = 15,
};

/// Plays the player's first room-extension clip during the saloon dialogue scenes.
///
/// Clip 48 selects extension entry 1. Interpolates over 15 frames and requests
/// disabled world collision. The event dispatcher copies this request and
/// replaces source.index with the equipped weapon's bank before dispatch;
/// its stored value 1 is unused. The extension data stays live with the room.
static AnimationPlayRequest _gDryfieldNightSaloonGRPlayerDialogueClip48Request = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_SALOON_G_R_PLAYER_DIALOGUE_SET,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = DRYFIELD_NIGHT_SALOON_G_R_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_night_saloon_g_r_8018397C = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183990 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_801839A4 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_801839B8 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_801839CC = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_801839E0 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_801839F4 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A08 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A1C = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A30[3] = {
    { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A6C = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A80 = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183A94 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183AA8 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_saloon_g_r_80183ABC = { { 950, 0, -180, 0 }, { 0, 530, 0, 0 } };

ActorTransform D_dryfield_night_saloon_g_r_80183AD4 = { { 1640, 0, 510, 0 }, { 0, 530, 0, 0 } };

ActorTransform D_dryfield_night_saloon_g_r_80183AEC = { { 2150, 0, 545, 0 }, { 0, 470, 0, 0 } };

ActorTransform D_dryfield_night_saloon_g_r_80183B04 = { { 2150, 0, 545, 0 }, { 0, 470, 0, 0 } };

ActorTransform D_dryfield_night_saloon_g_r_80183B1C = { { 4110, 0, 1730, 0 }, { 0, -1024, 0, 0 } };

GameActorMoveAnim D_dryfield_night_saloon_g_r_80183B34 = { 62, 48 };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183B3C[2] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE },
};

enum {
    DRYFIELD_NIGHT_SALOON_G_R_KYLE_ANIMATION_BANK = 0,
    DRYFIELD_NIGHT_SALOON_G_R_KYLE_INITIAL_CLIP   = 2,
};

/// Restarts Kyle Madigan's initial pose during the night-saloon dialogue scenes.
///
/// Placed actor 0 borrows this bank-0, clip-2 request during synchronous
/// dispatch. Playback resets without blending; this actor ignores the duration
/// and collision words.
static AnimationPlayRequest _gDryfieldNightSaloonGRKyleInitialPoseRequest = {
    .source               = { .index = DRYFIELD_NIGHT_SALOON_G_R_KYLE_ANIMATION_BANK },
    .animationId          = DRYFIELD_NIGHT_SALOON_G_R_KYLE_INITIAL_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183B78 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183B8C = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183BA0 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183BB4 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183BC8 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 25, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183BDC = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_dryfield_night_saloon_g_r_80183BF0 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C04 = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C18 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C2C = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C40 = { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C54 = { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183C68 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_saloon_g_r_80183C7C = { { 2670, 0, 1520, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_dryfield_night_saloon_g_r_80183C94[118] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_8018394C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183ABC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183AD4 } }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B34 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53120006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53120007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_8018397C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183990 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53120008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A1C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53120009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_801839B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183BDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A94 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightSaloonGRSetRoom }, { .value = DRYFIELD_NIGHT_SALOON_G_R_ROOM_AFTER_CUTSCENE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_saloon_g_r_801847A4[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183ABC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightSaloonGRSetRoom }, { .value = DRYFIELD_NIGHT_SALOON_G_R_ROOM_AFTER_CUTSCENE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_saloon_g_r_801848DC[25] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_8018394C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183AEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5312000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183A6C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_saloon_g_r_80184B34[21] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_8018394C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183AA8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_saloon_g_r_80184D2C[20] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_8018394C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183C7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightSaloonGRPlayerDialogueClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5312000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gDryfieldNightSaloonGRKyleInitialPoseRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// MIDI sequence IDs paired with the saloon's numbered jukebox labels.
///
/// The menu loads each ID as file 40100 + ID before starting MIDI playback.
enum {
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_TOWER_RENDEZVOUS        = 9,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_OUT_OF_PHASE_2          = 17,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAZY_KING              = 20,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_AMBUSH                  = 21,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_QUADRUMANOUS_LEADER     = 22,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_EAGER_FOR_BLOOD         = 23,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CURSE_YOUR_FATE         = 31,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LA_MAZE                 = 35,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LIGHTNING_OPERATION     = 36,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LAZING_AWAY_THE_MORNING = 37,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GHOST_TOWN              = 41,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_WILD_HUNT               = 42,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_THE_VAGRANTS            = 43,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_SNOOPER                 = 44,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_ROCK_AND_FIRE           = 45,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_HEAVEN_SENT_KILLER      = 49,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_REQUIEM                 = 58,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_WISHWASH                = 59,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_PICK_UP_THE_GAUNTLET    = 60,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_DARK_VOICE_OF_THE_HEART = 61,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAWLING_WASTE_EMPEROR  = 66,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_YELLOW_RAIN             = 67,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_HUNTERS_MOON            = 74,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_MAN_MADE_NATURE         = 76,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GAZING_INTO_THE_VOID    = 77,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_FOOLS_PARADISE          = 78,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_REBEL_FORCES            = 82,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_KILLING_FIELD           = 83,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_MENTAL_AGONY            = 89,
    DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GENIC_REACTOR           = 93,
};

/// Three training-mode jukebox rows for a cleared Replay save (gameMode 0).
///
/// IDs and NUL-terminated labels remain live with this overlay. The shared
/// callback selects rows 0..2 when the training-mode predicate is true.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxTrainingReplayTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAZY_KING, _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_EAGER_FOR_BLOOD, _gDryfieldNightSaloonGRJukeboxRow2EagerForBloodLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_HEAVEN_SENT_KILLER, _gDryfieldNightSaloonGRJukeboxRow3HeavenSentKillerLabel },
};

/// Three training-mode jukebox rows for a cleared Bounty save (gameMode 1).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..2
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxTrainingBountyTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAZY_KING, _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_PICK_UP_THE_GAUNTLET, _gDryfieldNightSaloonGRJukeboxRow2PickUpTheGauntletLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAWLING_WASTE_EMPEROR, _gDryfieldNightSaloonGRJukeboxRow3CrawlingWasteEmperorLabel },
};

/// Three training-mode jukebox rows for a cleared Scavenger save (gameMode 2).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..2
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxTrainingScavengerTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAZY_KING, _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_QUADRUMANOUS_LEADER, _gDryfieldNightSaloonGRJukeboxRow2QuadrumanousLeaderLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_HUNTERS_MOON, _gDryfieldNightSaloonGRJukeboxRow3HuntersMoonLabel },
};

/// Three training-mode jukebox rows for a cleared Nightmare save (gameMode 3).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..2
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxTrainingNightmareTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_YELLOW_RAIN, _gDryfieldNightSaloonGRJukeboxRow1YellowRainLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_REBEL_FORCES, _gDryfieldNightSaloonGRJukeboxRow2RebelForcesLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GENIC_REACTOR, _gDryfieldNightSaloonGRJukeboxRow3GenicReactorLabel },
};

/// Three training-mode jukebox rows before the save's first clear, ignoring gameMode.
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..2
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxTrainingUnclearedTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CRAZY_KING, _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_AMBUSH, _gDryfieldNightSaloonGRJukeboxRow2AmbushLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_PICK_UP_THE_GAUNTLET, _gDryfieldNightSaloonGRJukeboxRow3PickUpTheGauntletLabel },
};

/// Four regular jukebox rows for a cleared Replay save (gameMode 0).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..3
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxRegularReplayTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GHOST_TOWN, _gDryfieldNightSaloonGRJukeboxRow1GhostTownLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_ROCK_AND_FIRE, _gDryfieldNightSaloonGRJukeboxRow2RockAndFireLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_REQUIEM, _gDryfieldNightSaloonGRJukeboxRow3RequiemLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_DARK_VOICE_OF_THE_HEART, _gDryfieldNightSaloonGRJukeboxRow4DarkVoiceOfTheHeartLabel },
};

/// Four regular jukebox rows for a cleared Bounty save (gameMode 1).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..3
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxRegularBountyTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LA_MAZE, _gDryfieldNightSaloonGRJukeboxRow1LaMazeLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LIGHTNING_OPERATION, _gDryfieldNightSaloonGRJukeboxRow2LightningOperationLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_WILD_HUNT, _gDryfieldNightSaloonGRJukeboxRow3WildHuntLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_SNOOPER, _gDryfieldNightSaloonGRJukeboxRow4SnooperLabel },
};

/// Four regular jukebox rows for a cleared Scavenger save (gameMode 2).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..3
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxRegularScavengerTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_CURSE_YOUR_FATE, _gDryfieldNightSaloonGRJukeboxRow1CurseYourFateLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_WISHWASH, _gDryfieldNightSaloonGRJukeboxRow2WishwashLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_MENTAL_AGONY, _gDryfieldNightSaloonGRJukeboxRow3MentalAgonyLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GENIC_REACTOR, _gDryfieldNightSaloonGRJukeboxRow4GenicReactorLabel },
};

/// Four regular jukebox rows for a cleared Nightmare save (gameMode 3).
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..3
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxRegularNightmareTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_MAN_MADE_NATURE, _gDryfieldNightSaloonGRJukeboxRow1ManMadeNatureLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_GAZING_INTO_THE_VOID, _gDryfieldNightSaloonGRJukeboxRow2GazingIntoTheVoidLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_FOOLS_PARADISE, _gDryfieldNightSaloonGRJukeboxRow3FoolsParadiseLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_KILLING_FIELD, _gDryfieldNightSaloonGRJukeboxRow4KillingFieldLabel },
};

/// Four regular jukebox rows before the save's first clear, ignoring gameMode.
///
/// Each row pairs a MIDI ID with its numbered NUL-terminated label; rows 0..3
/// and their labels remain live with the overlay while the menu borrows them.
static JukeboxTrack _gDryfieldNightSaloonGRJukeboxRegularUnclearedTracks[] = {
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_TOWER_RENDEZVOUS, _gDryfieldNightSaloonGRJukeboxRow1TowerRendezvousLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_THE_VAGRANTS, _gDryfieldNightSaloonGRJukeboxRow2TheVagrantsLabel },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_OUT_OF_PHASE_2, _gDryfieldNightSaloonGRJukeboxRow3OutOfPhase2Label },
    { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SEQUENCE_LAZING_AWAY_THE_MORNING, _gDryfieldNightSaloonGRJukeboxRow4LazingAwayTheMorningLabel },
};

/// Shared row drawer used for every jukebox track, independent of item count.
///
/// The list enables USER_INTERFACE_LIST_SHARED_ROW_CALLBACK before dispatch,
/// so all three/four reachable rows use entry zero. Borrowed for the menu's life.
static UiListRowCallback _gDryfieldNightSaloonGRJukeboxRowCallbacks[] = {
    _jukeboxDrawRow,
};

enum { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_ROW_HEIGHT_PIXELS = 17 };

/// Mutable jukebox selection, scrolling and row-drawing state for the open panel.
///
/// Starts with one row; menu initialization sets three training or four regular
/// rows, resets selection and scrolling, and enables the shared callback.
/// Navigation clamps at either end. Row spacing is 17 panel-relative pixels.
/// The overlay owns the singleton through the menu and host's closing delay.
static UiList _gDryfieldNightSaloonGRJukeboxList = {
    .rowCallbacks    = _gDryfieldNightSaloonGRJukeboxRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_ROW_HEIGHT_PIXELS,
};

UiObjectDesc gJukeboxPanelDesc = { USER_INTERFACE_PANEL_TITLE_STYLE, { -112, -64, 224, 128 }, 48, 0, TASK_BODY_NONE, 192, _dryfieldNightSaloonGRJukeboxMenuTask, 0 };

TaskDesc D_dryfield_night_saloon_g_r_80185068 = { { { TASK_BODY_NONE, 192 } }, _jukeboxHostTask, { .value = 0 } };

/// Night-saloon flare centres, beam endpoints and shaft guides in game-coordinate units.
///
/// Points 0..10 and 20..27 are world-space flare centres. Points 12/13 are
/// local-space beam endpoints; 14/17 are the shaft roots, with local guides
/// 15/18 and 16/19 whose root offsets are extended fourfold. Local points use
/// the glow task's coordinate. Point 11 is unread; its role and frame are unproven.
/// XYZ are signed halfwords; the fourth SDK component is unused and zero.
/// The table stays live with this overlay and drawing leaves it unchanged.
static SVECTOR _gDryfieldNightSaloonGREffectPoints[28] = {
    { 4915, -1870, -727, 0 },
    { 4915, -1870, -2339, 0 },
    { 4915, -1870, -4188, 0 },
    { -3905, -1870, -4460, 0 },
    { -100, -1870, 4785, 0 },
    { 1100, -1870, 4785, 0 },
    { 3500, -2097, 3000, 0 },
    { 3500, -2097, 2000, 0 },
    { 3500, -2097, 1000, 0 },
    { 3500, -2097, 0, 0 },
    { 4200, -2097, -500, 0 },
    { 4983, -2211, 4240, 0 },
    { 4983, -2211, 4467, 0 },
    { 4983, -2211, 4013, 0 },
    { 700, -1640, 1680, 0 },
    { 700, -1520, 1520, 0 },
    { 700, -1520, 1840, 0 },
    { -750, -1640, 1680, 0 },
    { -750, -1520, 1520, 0 },
    { -750, -1520, 1840, 0 },
    { -3900, -1840, -2430, 0 },
    { -3900, -1840, -420, 0 },
    { -3900, -1840, 1630, 0 },
    { -2600, -2570, -2500, 0 },
    { -2600, -2570, 1900, 0 },
    { -300, -2570, -300, 0 },
    { 2000, -2570, -2500, 0 },
    { 2000, -2570, 1900, 0 },
};

/// View visibility for the night-saloon flares, beam and twin shafts.
///
/// Bit `1 << view` admits logical view 1..13; zero admits none. Entries 0..10
/// gate the matching flare points, 11 gates the beam, 12 the twin shafts,
/// and 13..20 gate points 20..27. Signed halfword loads promote before testing.
/// Entry 21 is unread and its interpretation is unproven; retain its bytes.
static s16 _gDryfieldNightSaloonGREffectViewMasks[22] = {
    (1 << 7) | (1 << 11),                       // Flare point 0
    (1 << 2) | (1 << 3) | (1 << 11),            // Flare point 1
    (1 << 2),                                   // Flare point 2
    (1 << 3) | (1 << 10),                       // Flare point 3
    (1 << 4) | (1 << 5) | (1 << 9),             // Flare point 4
    (1 << 4) | (1 << 5) | (1 << 9),             // Flare point 5
    (1 << 4) | (1 << 7) | (1 << 9),             // Flare point 6
    (1 << 4) | (1 << 7) | (1 << 9) | (1 << 11), // Flare point 7
    (1 << 7) | (1 << 9) | (1 << 11),            // Flare point 8
    (1 << 7) | (1 << 11),                       // Flare point 9
    (1 << 7) | (1 << 11),                       // Flare point 10
    (1 << 4) | (1 << 7) | (1 << 9),             // Beam endpoints 12/13
    (1 << 4) | (1 << 5) | (1 << 9),             // Twin shafts, points 14..19
    (1 << 10),                                  // Flare point 20
    (1 << 10),                                  // Flare point 21
    (1 << 10),                                  // Flare point 22
    (1 << 10),                                  // Flare point 23
    (1 << 10),                                  // Flare point 24
    (1 << 10),                                  // Flare point 25
    0,                                          // Flare point 26
    (1 << 9),                                   // Flare point 27
    -3540,                                      // Unread serialized halfword; role unproven
};

WorldCoordRoomLighting gDryfieldNightSaloonGRRoomLighting[2] = {
    { .lights = &_gDryfieldNightSaloonGRRoomLights, .ambientTable = NULL },
    { .lights = &_gDryfieldNightSaloonGRRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources D_dryfield_night_saloon_g_r_80185190[2] = {
    { &_gDryfieldNightSaloonGRCollisionGrid, _gDryfieldNightSaloonGRViewBoundaryTriggers, _gDryfieldNightSaloonGRActionTriggers, D_dryfield_night_saloon_g_r_80188E18 },
    { &_gDryfieldNightSaloonGRCollisionGrid, _gDryfieldNightSaloonGRViewBoundaryTriggers, _gDryfieldNightSaloonGRActionTriggers, D_dryfield_night_saloon_g_r_80188E18 },
};

u8* gDryfieldNightSaloonGRViewMaps[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gDryfieldNightSaloonGRViewCounts[DRYFIELD_NIGHT_SALOON_G_R_ROOM_COUNT] = {
    DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT,
    DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT,
};

DirectionWarpEntry D_dryfield_night_saloon_g_r_801851BC[2] = {
    { { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, 0x53120002, 0x53120001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, 0x53120004, 0x53120003, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 478 },
};

enum {
    DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_X = 3,
    DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_Z = 4,
};

/// Nine room-space collision-plane normals, with 4096 representing one unit.
static SVECTOR _gDryfieldNightSaloonGRCollisionGridNormals[] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_normals.inc"
};

/// 116 collision-mesh vertices in signed room-local game coordinates.
static SVECTOR _gDryfieldNightSaloonGRCollisionGridVerts[] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_verts.inc"
};

/// 61 indexed collision quads referencing the grid's vertex and normal pools.
static WorldCollisionGridFace _gDryfieldNightSaloonGRCollisionGridFaces[] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_faces.inc"
};

/// Serialized cell face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// Includes all 280 halfwords between the face pool and cell-pointer table;
/// only the sentinel-terminated runs reached by the table are traversed.
static s16 _gDryfieldNightSaloonGRCollisionGridCells[] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_cells.inc"
};

// The generated initializer supplies halfword offsets into this grid's cell pool.
#define GRID_CELL(i) (&_gDryfieldNightSaloonGRCollisionGridCells[i])
/// X-major 3-by-4 table of borrowed, sentinel-terminated cell face-index lists.
static s16* _gDryfieldNightSaloonGRCollisionGridTable[DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_X * DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_Z] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_table.inc"
};
#undef GRID_CELL

/// Collision mesh and spatial index shared by both night-saloon room entries.
///
/// Room setup binds viewCoord to the current view. Positions use game units;
/// the 4000-unit cells cover X from -4500 and Z from -5400 in a 3-by-4 grid.
/// The descriptor and its borrowed pools remain live while the overlay is loaded.
static WorldCollisionGrid _gDryfieldNightSaloonGRCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightSaloonGRCollisionGridNormals,
    .vertices    = _gDryfieldNightSaloonGRCollisionGridVerts,
    .faces       = _gDryfieldNightSaloonGRCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightSaloonGRCollisionGridTable,
    .xBias       = 4500,
    .zBias       = 5400,
    .cellCountX  = DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_NIGHT_SALOON_G_R_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightSaloonGRCollisionGridFaces),
};

ViewCamera gDryfieldNightSaloonGRViewCameras[DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, -680 } }, 519 },
    { { { { -3183, 0, -2576 }, { -1148, 3666, 1418 }, { 2306, 1824, -2850 } }, { 315, 2637, 16 } }, 257 },
    { { { { -4045, 0, -638 }, { -348, 3430, 2210 }, { 534, 2237, -3388 } }, { 724, 3602, -2788 } }, 230 },
    { { { { 4063, 0, -511 }, { -261, 3522, -2074 }, { 439, 2090, 3494 } }, { 628, 3274, 3211 } }, 230 },
    { { { { 3790, 0, -1551 }, { -715, 3635, -1746 }, { 1377, 1887, 3364 } }, { 1033, 2561, -668 } }, 230 },
    { { { { -171, 0, 4092 }, { 3010, 2774, 126 }, { -2771, 3013, -116 } }, { -1881, 2483, -5950 } }, 188 },
    { { { { 3128, 0, -2643 }, { -887, 3858, -1050 }, { 2490, 1375, 2946 } }, { -1421, 2204, 1801 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -620, 1520, -5560 } }, 230 },
    { { { { 4089, 0, -236 }, { 96, 3738, 1671 }, { 215, -1674, 3731 } }, { -1891, 732, 608 } }, 230 },
    { { { { -627, 0, 4047 }, { 323, 4082, 50 }, { -4034, 326, -625 } }, { -4164, 1341, -1329 } }, 230 },
    { { { { -110, 0, -4094 }, { -247, 4088, 6 }, { 4087, 247, -109 } }, { -415, 1531, -768 } }, 230 },
    { { { { 702, 0, 4035 }, { 1935, 3594, -337 }, { -3540, 1964, 616 } }, { 2150, 1500, -3200 } }, 289 },
    { { { { 2919, 0, 2873 }, { 1429, 3553, -1451 }, { -2492, 2037, 2532 } }, { 1395, 2500, -2735 } }, 257 },
};

enum { DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR = 128 };

/// View 1's decoded-image selector and empty sprite batch list.
///
/// The leading zero-count record precedes the end marker. Its retained address
/// in the view's source slot supplies no source elements.
static SpriteBatch _gDryfieldNightSaloonGRView1SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 17 mutable sprite rectangles and sorting depths for night-saloon view 2.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 48, 550, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 88, 550, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 104, 500, { .fields = { 56, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 16, 96, 500, { .fields = { 40, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 16, 64, 525, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 72, 104, 500, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, 80, 500, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -8, 850, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 850, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 40, 875, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 875, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1050, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1050, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 8, 1075, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -8, 1025, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 1075, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -16, 1075, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView2SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView2SpriteSources), .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 14 mutable sprite rectangles and sorting depths for night-saloon view 3.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 1200, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, 8, 1050, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, 16, 975, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -32, -16, 1250, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -32, 16, 975, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 48, 112, 719, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 16, 104, 730, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, 104, 516, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, 96, 522, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 96, 521, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 529, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -128, 88, 529, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -136, 96, 532, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, 112, 527, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView3SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView3SpriteSources) - 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 70 mutable sprite rectangles and sorting depths for night-saloon view 4.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -112, 1250, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -112, 1250, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -56, 1250, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 1325, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -32, 1325, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -32, -32, 1375, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, -16, 1325, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1375, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 1325, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1325, { .fields = { 96, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 0, 1375, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -16, 1375, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, 16, 1375, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, -32, 1350, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -16, 1325, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 1325, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -16, 1350, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 0, 1325, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 16, 1250, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -152, 24, 1250, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 112, 40 } }, -8, 80, 705, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -8, 48, 798, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 8, 925, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 32 } }, -24, 16, 1000, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, 48, 925, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 64, 925, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 32, 925, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -112, -120, 1775, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -104, -112, 1843, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -96, -72, 1758, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 40, 48 } }, -80, -80, 1981, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 40, 40 } }, -80, -120, 1847, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -8, -120, 1873, { .fields = { 0, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -96, 2291, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, -120, 1855, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -96, 1921, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -72, 2025, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, -48, 2105, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -40, -48, 1930, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -72, 1985, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -96, 1900, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -32, 1833, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -48, 1615, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 88, -64, 1862, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, -64, 2213, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 80, -40, 1611, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 96, -64, 1775, { .fields = { 32, 24 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -40, 1588, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -24, 1724, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 104, -16, 1623, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -40, 1414, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -64, 1727, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -56, 1608, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -32, 1377, { .fields = { 24, 24 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 112, -8, 1540, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, 0, 1455, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -24, 1509, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -48, 1480, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -48, 1424, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 128, -16, 1229, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 8, 1378, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, 8, 1327, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -16, 1213, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -40, 1371, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, -40, 1323, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 144, -8, 1292, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 144, 16, 1059, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 152, 24, 997, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 0, 1065, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 152, -32, 1251, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 4's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView4SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 20, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 20, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 27, .spriteCount = 14, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 41, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView4SpriteSources) - 41, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 63 mutable sprite rectangles and sorting depths for night-saloon view 5.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 250, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 250, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 16, -24, 250, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -88, 250, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 128, -48, 250, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 250, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 250, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 250, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 32, 250, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -32, 40, 250, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -48, 1143, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 8, 1253, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -40, 1150, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -8, 1175, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -88, 934, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -40, 999, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, 8, 932, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -40, 1044, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, 8, 1011, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 8, 1116, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -40, 1208, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -40, 1046, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -40, 1090, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -24, 1183, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -80, -120, 940, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -128, -120, 917, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, -88, 932, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -88, 921, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -112, -72, 1008, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, -88, 1024, { .fields = { 24, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -72, 1083, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -88, 1250, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -72, 1312, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -56, 1325, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -40, 1375, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -24, 1500, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -16, 1500, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -8, 1500, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -88, 1250, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -72, 1250, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -56, 1312, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -40, 1325, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -24, 1337, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -16, 1375, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, -8, 1375, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 0, 1500, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -88, 1250, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1250, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -56, 1250, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -40, 1325, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 1375, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -8, 1375, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -96, 88, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 80, 500, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 72, 500, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, 64, 500, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 24, 56, 500, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 56, 48, 500, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 40, 500, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 32, 500, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 40, 500, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 80, 88, 500, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 500, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 5's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView5SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 10, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 10, .spriteCount = 42, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 52, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView5SpriteSources) - 52, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 91 mutable sprite rectangles and sorting depths for night-saloon view 6.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView6SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 375, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 375, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 375, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 375, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 72, 375, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, 80, 375, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 80, 375, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 80, 250, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 72, 250, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 250, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 250, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 250, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 250, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 250, { .fields = { 112, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 80, 250, { .fields = { 48, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 250, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 156, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 742, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -40, 750, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 689, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 843, { .fields = { 104, 88 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -64, 547, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -64, 541, { .fields = { 0, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -64, 589, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 598, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -40, 645, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -40, 723, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -40, 686, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 751, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -40, 576, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 56, 549, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 24, 460, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 384, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 0, 386, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -8, 354, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -24, 301, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -40, 295, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 293, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -64, 276, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 261, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -104, 247, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 299, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -120, 258, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -104, 267, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, -120, 325, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -120, 625, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -24, 641, { .fields = { 0, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -24, 748, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -32, 698, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -48, 649, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -64, 866, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -88, 919, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -120, 827, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, -120, 492, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -88, 564, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -96, 625, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -120, 423, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -120, 453, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -120, 490, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 246, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -48, 259, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -32, 271, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -16, 272, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -8, 307, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 8, 328, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -16, 239, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 252, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 32, 324, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 260, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 64, 262, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 625, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 615, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -24, 625, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 625, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -48, 625, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 625, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -80, 625, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, -96, 625, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 731, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -64, 625, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 625, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -72, 625, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 358, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 32, 386, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 48, 420, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 448, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 492, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 64, 613, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 290, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 6's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView6SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 19, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 19, .spriteCount = 13, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 32, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView6SpriteSources) - 32, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 23 mutable sprite rectangles and sorting depths for night-saloon view 7.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 16, 625, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -8, 625, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 72, 40, 625, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 8, 500, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 56, 500, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, -8, 500, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, 40, 500, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 0, 40, 500, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, -16, 500, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -32, 16, 625, { .fields = { 72, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -32, 625, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -64, -40, 975, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, 0, 975, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -128, -32, 1175, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -40, 1125, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -8, 1125, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -80, 1550, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -32, 1550, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -48, -80, 1550, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -32, 1550, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, 8, 950, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 8, 987, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 40, 1050, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 7's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView7SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 16, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 16, .spriteCount = 4, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 20, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView7SpriteSources) - 20, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// View 8's decoded-image selector and empty sprite batch list.
///
/// The leading zero-count record precedes the end marker. Its retained address
/// in the view's source slot supplies no source elements.
static SpriteBatch _gDryfieldNightSaloonGRView8SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 13 mutable sprite rectangles and sorting depths for night-saloon view 9.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 500, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 72, 525, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 88, 500, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 64, 525, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 80, 500, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 525, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 500, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 450, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 0, 350, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, -24, 362, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -56, 375, { .fields = { 112, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 387, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -88, 387, { .fields = { 120, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 9's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView9SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 7, .spriteCount = 1, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 8, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView9SpriteSources) - 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 119 mutable sprite rectangles and sorting depths for night-saloon view 10.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView10SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 130, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 88, 130, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 72, 130, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 88, 130, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 88, 135, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 80, 149, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 137, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 80, 135, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 32, 152, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 144, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 56, 141, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 40, 132, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 56, 132, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 40, 129, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 129, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 56, 139, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -8, 140, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 144, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 140, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 142, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 24, 140, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 737, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 140, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 40, 140, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 40, 142, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 139, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 72, 140, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 140, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 72, 141, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 88, 141, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 88, 142, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 154, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 64, 150, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 151, { .fields = { 16, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 292, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 159, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 72, 156, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 56, 236, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 32, 152, { .fields = { 24, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 48, 146, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 147, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 148, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 96, 148, { .fields = { 0, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 72, 191, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 152, { .fields = { 24, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 48, 146, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 64, 146, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 80, 148, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 147, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 150, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 151, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 169, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 152, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 24, 152, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 152, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 48, 156, { .fields = { 80, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 64, 157, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 148, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 64, 146, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 146, { .fields = { 0, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 149, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 149, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 40, 149, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 56, 144, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 72, 144, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 926, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 32, 1258, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 149, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 64, 146, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 56, 153, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 155, { .fields = { 64, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, 40, 300, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, 40, 300, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -16, 40, 300, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, 40, 250, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 48, 40, 300, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 40, 250, { .fields = { 96, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 40, 250, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 129, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 96, 130, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 112, 131, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 80, 132, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 96, 133, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 133, { .fields = { 8, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 141, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 96, 141, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 112, 141, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 80, 183, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 96, 156, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 112, 144, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 139, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 140, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 144, { .fields = { 0, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 80, 150, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 96, 150, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 112, 144, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 152, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 152, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 112, 144, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 153, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 96, 154, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 112, 144, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 80, 147, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 96, 148, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 112, 144, { .fields = { 8, 16 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 80, 183, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 96, 159, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 112, 141, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 80, 150, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 96, 151, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 145, { .fields = { 8, 24 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 80, 145, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 96, 146, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 112, 145, { .fields = { 8, 32 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 148, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 96, 150, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 112, 145, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 80, 177, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 104, 150, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 10's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView10SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 71, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 71, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView10SpriteSources) - 71, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// 12 mutable sprite rectangles and sorting depths for night-saloon view 11.
///
/// Positions and dimensions are pixels; depths are unscaled OT values.
/// The view's batches select elements of this array while the overlay is loaded.
static SpriteSource _gDryfieldNightSaloonGRView11SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -96, 56, 612, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 56, 800, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 8, 800, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, 0, 800, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, 56, 800, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 8, 800, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 56, 800, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 8, 800, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 56, 800, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 800, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 40, 800, { .fields = { 24, 128 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 72, 800, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 11's sprite ranges, preceded by a decoded-image background selector.
///
/// Ranges cover the complete source array and end at `SPRITE_BATCH_END`.
/// The two serialized bytes in each batch retain their unproven interpretation.
static SpriteBatch _gDryfieldNightSaloonGRView11SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 1, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 1, .spriteCount = ARRAY_SIZE(_gDryfieldNightSaloonGRView11SpriteSources) - 1, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// View 12's decoded-image selector and empty sprite batch list.
///
/// A zero-count leading record requests image strips; the following record
/// ends the ranges. The view also retains this address in its empty source slot.
/// No source elements or clipping areas are supplied.
static SpriteBatch _gDryfieldNightSaloonGRView12SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// View 13's decoded-image selector and empty sprite batch list.
///
/// A zero-count leading record requests image strips; the following record
/// ends the ranges. The view also retains this address in its empty source slot.
/// No source elements or clipping areas are supplied.
static SpriteBatch _gDryfieldNightSaloonGRView13SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gDryfieldNightSaloonGRSpriteViews[DRYFIELD_NIGHT_SALOON_G_R_VIEW_COUNT] = {
    { { .empty = _gDryfieldNightSaloonGRView1SpriteBatches }, _gDryfieldNightSaloonGRView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView2SpriteSources }, _gDryfieldNightSaloonGRView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView3SpriteSources }, _gDryfieldNightSaloonGRView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView4SpriteSources }, _gDryfieldNightSaloonGRView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView5SpriteSources }, _gDryfieldNightSaloonGRView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView6SpriteSources }, _gDryfieldNightSaloonGRView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView7SpriteSources }, _gDryfieldNightSaloonGRView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightSaloonGRView8SpriteBatches }, _gDryfieldNightSaloonGRView8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView9SpriteSources }, _gDryfieldNightSaloonGRView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView10SpriteSources }, _gDryfieldNightSaloonGRView10SpriteBatches, NULL },
    { { .elements = _gDryfieldNightSaloonGRView11SpriteSources }, _gDryfieldNightSaloonGRView11SpriteBatches, NULL },
    { { .empty = _gDryfieldNightSaloonGRView12SpriteBatches }, _gDryfieldNightSaloonGRView12SpriteBatches, NULL },
    { { .empty = _gDryfieldNightSaloonGRView13SpriteBatches }, _gDryfieldNightSaloonGRView13SpriteBatches, NULL },
};

/// Seven point lights shared by both night-saloon room entries.
///
/// Positions and falloff distances use world units; RGB uses Q12 (`ONE` full).
/// Every light admits all views. Coordinate composition and lighting queries
/// update these records, which stay live only while the room overlay is loaded.
static WorldCoordPointLight _gDryfieldNightSaloonGRPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1392, 1556, 1720 }, { 0, 0 } }, 100000, 100000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 940, -1500, 5040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3768, 3522, 2703 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1850, -1500, -3210 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 2621 }, { 0, 0 } }, 1500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3480, -1500, -3530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3932, ONE }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3780, -1600, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3112, 2457 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4750, -1400, 4320 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2490, -1000, 3100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3276, 2375 }, { 0, 0 } }, 1000, 3000 },
};

/// Point-light collection shared by both night-saloon rooms, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightSaloonGRRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightSaloonGRPointLights),
    .pointLights           = _gDryfieldNightSaloonGRPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Sixteen directed camera-switch quads shared by both night-saloon rooms.
///
/// parameter0/parameter1 are source/destination logical views 2..7. The last
/// quad carries LAST and is included in setup. Room setup binds coordinates,
/// links and enables these mutable records, which live with the room overlay.
/// Origins, corners and radii use game units; normals use 4096 per unit.
static WorldCollisionTrigger _gDryfieldNightSaloonGRViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { 2633, -1376, -1224, 0 }, { { -2531, -2400, -372, 0 }, { 2524, -2400, 366, 0 }, { -2531, 2400, -372, 0 }, { 2524, 2400, 366, 0 } }, { 592, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 3500, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1248, -1346, 0 }, { { 2519, -2272, 364, 0 }, { -2534, -2272, -375, 0 }, { 2519, 2272, 364, 0 }, { -2534, 2272, -375, 0 } }, { -594, 0, 4058, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1685, -1152, -59, 0 }, { { 1038, -2176, 153, 0 }, { -1037, -2176, -153, 0 }, { 1038, 2176, 153, 0 }, { -1037, 2176, -153, 0 } }, { -603, 0, 4071, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1659, -1280, 60, 0 }, { { -1102, -2304, -152, 0 }, { 1103, -2304, 152, 0 }, { -1102, 2304, -152, 0 }, { 1103, 2304, 152, 0 } }, { 561, 0, -4072, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1801, -1520, 2996, 0 }, { { -878, -2544, -853, 0 }, { 878, -2544, 853, 0 }, { -878, 2544, -853, 0 }, { 878, 2544, 853, 0 } }, { 2854, 0, -2940, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1185, -1632, 3294, 0 }, { { -299, -2656, 1167, 0 }, { 295, -2656, -1172, 0 }, { -299, 2656, 1167, 0 }, { 295, 2656, -1172, 0 } }, { -3977, 0, -1011, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1824, -1456, 2896, 0 }, { { 698, -2480, 685, 0 }, { -711, -2480, -703, 0 }, { 698, 2480, 685, 0 }, { -711, 2480, -703, 0 } }, { -2884, 0, 2926, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1377, -1344, 3391, 0 }, { { 300, -2368, -1106, 0 }, { -309, -2368, 1096, 0 }, { 300, 2368, -1106, 0 }, { -309, 2368, 1096, 0 } }, { 3955, 0, 1093, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 431, -1552, 5183, 0 }, { { -1068, -2576, -9, 0 }, { 1069, -2576, 10, 0 }, { -1068, 2576, -9, 0 }, { 1069, 2576, 10, 0 } }, { 35, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 383, -1376, 5055, 0 }, { { 1069, -2400, 0, 0 }, { -1069, -2400, 0, 0 }, { 1069, 2400, 0, 0 }, { -1069, 2400, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2906, -1200, 4312, 0 }, { { 1, -2224, -1068, 0 }, { -1, -2224, 1068, 0 }, { 1, 2224, -1068, 0 }, { -1, 2224, 1068, 0 } }, { 4096, 0, 3, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3066, -1216, 4376, 0 }, { { -1, -2240, 1068, 0 }, { 1, -2240, -1068, 0 }, { -1, 2240, 1068, 0 }, { 1, 2240, -1068, 0 } }, { -4104, 0, -6, 0 }, { 0, 0, 4096, 0 }, 2468, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 158, -1216, -3715, 0 }, { { 4, -2272, -2047, 0 }, { -13, -2272, 2033, 0 }, { 4, 2272, -2047, 0 }, { -13, 2272, 2033, 0 } }, { 4099, 0, 16, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 303, -1216, -3729, 0 }, { { 11, -2272, 1935, 0 }, { -21, -2272, -1950, 0 }, { 11, 2272, 1935, 0 }, { -21, 2272, -1950, 0 } }, { -4121, 0, 33, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2128, -1248, -193, 0 }, { { -1157, -2304, -193, 0 }, { 1157, -2304, 194, 0 }, { -1157, 2304, -193, 0 }, { 1157, 2304, 194, 0 } }, { 675, 0, -4046, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2144, -1280, -353, 0 }, { { 1157, -2304, 194, 0 }, { -1157, -2304, -193, 0 }, { 1157, 2304, 194, 0 }, { -1157, 2304, -193, 0 } }, { -678, 0, 4044, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-command and action keys stored in the interaction-trigger parameters.
enum {
    DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_COMMAND_JUKEBOX      = 4,
    DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE     = 7,
    DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_RELEASE_BARRIER = 1,
    DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_START_ENCOUNTER = 2,
    DRYFIELD_NIGHT_SALOON_G_R_PARKING_LOT_WARP_SLOTS       = (1 << 4) | 4,
    DRYFIELD_NIGHT_SALOON_G_R_KITCHEN_WARP_SLOTS           = (2 << 4) | 1,
};

/// Twenty-one interaction and transition quads shared by both night-saloon rooms.
///
/// Includes two exits, jukebox/dialogue and CAP interactions, five entry-scene
/// regions and the encounter's barrier-release/start regions at indices 13/14.
/// Index 13 is unlinked after barrier release. The last included record carries
/// LAST; setup binds, links and enables all records. Storage remains mutable
/// and live with the overlay. Geometry uses game units, normals 4096 per unit.
static WorldCollisionTrigger _gDryfieldNightSaloonGRActionTriggers[21] = {
    { NULL, NULL, NULL, { 2464, -48, -4720, 0 }, { { 624, 0, -272, 0 }, { 624, 0, 272, 0 }, { -624, 0, -272, 0 }, { -624, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, DRYFIELD_NIGHT_SALOON_G_R_PARKING_LOT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4480, -50, 4368, 0 }, { { -448, 0, -624, 0 }, { 448, 0, -624, 0 }, { -448, 0, 624, 0 }, { 448, 0, 624, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN, DRYFIELD_NIGHT_SALOON_G_R_KITCHEN_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3232, -64, 3392, 0 }, { { 624, 0, -880, 0 }, { 624, 0, 880, 0 }, { -624, 0, -880, 0 }, { -624, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_COMMAND_JUKEBOX, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -32, -64, 1728, 0 }, { { 2032, 0, -1392, 0 }, { 2032, 0, 1392, 0 }, { -2032, 0, -1392, 0 }, { -2032, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4076, 0, 401, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4048, -64, -4672, 0 }, { { 896, 0, -848, 0 }, { 896, 0, 848, 0 }, { -896, 0, -848, 0 }, { -896, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { -1380, 0, 3856, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6432, 0 }, { { 464, 0, -272, 0 }, { 464, 0, 272, 0 }, { -464, 0, -272, 0 }, { -464, 0, 272, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1472, -64, -1152, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, 288, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4560, -64, 3456, 0 }, { { 1088, 0, -272, 0 }, { 1088, 0, 272, 0 }, { -1088, 0, -272, 0 }, { -1088, 0, 272, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3424, -64, 4384, 0 }, { { 384, 0, -976, 0 }, { 384, 0, 976, 0 }, { -384, 0, -976, 0 }, { -384, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4688, -64, 5040, 0 }, { { 928, 0, -320, 0 }, { 928, 0, 320, 0 }, { -928, 0, -320, 0 }, { -928, 0, 320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 981, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_ENTRY_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2080, -64, 1328, 0 }, { { 880, 0, -1056, 0 }, { 880, 0, 1056, 0 }, { -880, 0, -1056, 0 }, { -880, 0, 1056, 0 } }, { 0, 4104, 0, 0 }, { -4092, 0, -202, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3968, -64, 1408, 0 }, { { 704, 0, -991, 0 }, { 704, 0, 992, 0 }, { -704, 0, -991, 0 }, { -704, 0, 992, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, 600, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 224, -64, 4176, 0 }, { { 2768, 0, -1024, 0 }, { 2768, 0, 1024, 0 }, { -2768, 0, -1024, 0 }, { -2768, 0, 1024, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 2941, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_RELEASE_BARRIER, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, 5648, 0 }, { { 1232, 0, -464, 0 }, { 1232, 0, 464, 0 }, { -1232, 0, -464, 0 }, { -1232, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_NIGHT_SALOON_G_R_TRIGGER_ROOM_START_ENCOUNTER, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2336, -64, 3552, 0 }, { { 1296, 0, -336, 0 }, { 1296, 0, 688, 0 }, { -1296, 0, -688, 0 }, { -1296, 0, 336, 0 } }, { 0, 4105, 0, 0 }, { 601, 0, -4052, 0 }, 1465, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4912, -64, 560, 0 }, { { 848, 0, -1216, 0 }, { 848, 0, 1216, 0 }, { -848, 0, -1216, 0 }, { -848, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1481, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4352, -64, 192, 0 }, { { 848, 0, -528, 0 }, { 848, 0, 528, 0 }, { -848, 0, -528, 0 }, { -848, 0, 528, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 997, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3936, -64, -2144, 0 }, { { 624, 0, -720, 0 }, { 624, 0, 720, 0 }, { -624, 0, -720, 0 }, { -624, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4896, -64, 2544, 0 }, { { 848, 0, -848, 0 }, { 848, 0, 848, 0 }, { -848, 0, -848, 0 }, { -848, 0, 848, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3904, -64, 1664, 0 }, { { 704, 0, -1983, 0 }, { 704, 0, 1984, 0 }, { -704, 0, -1983, 0 }, { -704, 0, 1984, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 2095, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_saloon_g_r_80188E18[2] = {
    { NULL, NULL, { -1056, -1392, 5216, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1, 0 },
    { NULL, NULL, { 2048, -1376, 5215, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_dryfield_night_saloon_g_r_80188E90[2] = {
    { 140, 356, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_135600_8013B0C4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Blood Suckler resources for night-saloon placement variant 2.
///
/// Entry 40 loads actor_104000 and selects descriptor 0 for both placements.
/// Placement entry IDs select these recipes; `AREA_PLACEMENT_END` ends the list.
/// The layout borrows this table and the actor descriptors while their overlays
/// stay loaded through resource loading and actor spawning.
static AreaResource _gDryfieldNightSaloonGRVariant2Resources[] = {
    { .entryId = 40, .fileNumber = 40, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor04000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Scavenger and Blood Suckler resources for night-saloon placement variant 3.
///
/// Entry 16 loads actor_101600; entry 40 loads actor_204000. Each uses descriptor 0.
/// Placement entry IDs select these recipes; `AREA_PLACEMENT_END` ends the list.
/// The layout borrows this table and the actor descriptors while their overlays
/// stay loaded through resource loading and actor spawning.
static AreaResource _gDryfieldNightSaloonGRVariant3Resources[] = {
    { .entryId = 16, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = 40, .fileNumber = 40, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant D_dryfield_night_saloon_g_r_80188EE4[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017BE58, D_dryfield_night_saloon_g_r_80188E90 },
    { D_map_dryfield_full_8017BE78, _gDryfieldNightSaloonGRVariant2Resources },
    { D_map_dryfield_full_8017BEA8, _gDryfieldNightSaloonGRVariant3Resources },
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

enum {
    DRYFIELD_NIGHT_SALOON_G_R_CLASS1_WALK_SOUND       = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_SALOON_G_R_CLASS1_RUN_SOUND        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_SALOON_G_R_CLASS2_AND_3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_NIGHT_SALOON_G_R_CLASS2_AND_3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Surface class 1's walk/run footstep bases; scripted jumps use the walk pair.
///
/// Type-1 requests select the loaded bank. Cue 1 adds one to these cue-2 bases;
/// companion playback adds 100. Surface records borrow this overlay's storage.
static WorldCollisionFootstepSounds _gDryfieldNightSaloonGRClass1FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_SALOON_G_R_CLASS1_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_SALOON_G_R_CLASS1_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_SALOON_G_R_CLASS1_WALK_SOUND,
};

/// Shared walk/run footstep bases for surface classes 2 and 3.
///
/// Type-1 requests select the loaded bank. Cue 1 adds one to these cue-2 bases;
/// companion playback adds 100. Surface records borrow this overlay's storage.
static WorldCollisionFootstepSounds _gDryfieldNightSaloonGRClass2And3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_SALOON_G_R_CLASS2_AND_3_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_SALOON_G_R_CLASS2_AND_3_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_SALOON_G_R_CLASS2_AND_3_WALK_SOUND,
};

/// Solid surface policy for classes 0 and 4..7, without surface footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightSaloonGRSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Solid surface class 1 with its walk/run footstep pair.
static WorldCollisionSurfaceProperties _gDryfieldNightSaloonGRClass1Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightSaloonGRClass1FootstepSounds,
};

/// Solid surface class 2 with the footstep pair shared with class 3.
static WorldCollisionSurfaceProperties _gDryfieldNightSaloonGRClass2Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightSaloonGRClass2And3FootstepSounds,
};

/// Surface class 3 passes probes and suppresses weapon impacts, while retaining pushback and footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightSaloonGRClass3Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightSaloonGRClass2And3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightSaloonGRSurfacePropertiesByClass[8] = {
    &_gDryfieldNightSaloonGRSilentSurface,
    &_gDryfieldNightSaloonGRClass1Surface,
    &_gDryfieldNightSaloonGRClass2Surface,
    &_gDryfieldNightSaloonGRClass3Surface,
    &_gDryfieldNightSaloonGRSilentSurface,
    &_gDryfieldNightSaloonGRSilentSurface,
    &_gDryfieldNightSaloonGRSilentSurface,
    &_gDryfieldNightSaloonGRSilentSurface,
};

RoomSavedViewStorage D_dryfield_night_saloon_g_r_80188FA4 = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 16, 15, 0 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `dryfieldNightSaloonGRRoomTask`: the entry tick
/// `_dryfieldNightSaloonGRInitializeRoom`, the idle state
/// `_dryfieldNightSaloonGRIdleState`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_saloon_g_r_8017D5DC = {
    { _dryfieldNightSaloonGRInitializeRoom, _dryfieldNightSaloonGRIdleState, taskKill },
};
static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Saves the live-save view and selects the jukebox's presentation view.
///
/// Requires the singleton saved-view slot free until the scene restores it.
/// Only the live save's view byte changes; read, replace and store order is
/// retained so the saved byte always describes the preceding room view.
static inline void _dryfieldNightSaloonGRSelectJukeboxView(void)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_VIEW = 12 };
    McSaveData* liveSave;
    u8          savedView;

    liveSave                                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    savedView                                 = liveSave->state.location.loc.view;
    liveSave->state.location.loc.view         = DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_VIEW;
    D_dryfield_night_saloon_g_r_80188FA4.view = savedView;
}

/// Presents the jukebox caption and menu, then restores the room view and control.
///
/// Starts in state zero after scripted player control is held. The singleton
/// saved-view slot must remain free until restoration: overlapping scenes would
/// overwrite it. Selects live-save view 12, hides actors/player/HUD, waits for
/// CAP command 19, queues the jukebox, then restores the view and visibility,
/// rearms interaction for ten updates and kills itself. Requires the room and
/// CAP resources through states 0..4.
static void _dryfieldNightSaloonGRJukeboxSceneTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_PREPARE           = 0,
        DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_WAIT_CAP          = 1,
        DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_OPEN              = 2,
        DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SETTLE            = 3,
        DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_RESTORE           = 4,
        DRYFIELD_NIGHT_SALOON_G_R_CAP_JUKEBOX               = 19,
        DRYFIELD_NIGHT_SALOON_G_R_INTERACTION_REARM_UPDATES = 10,
    };

    switch (task->state) {
        case DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_PREPARE:
            gGameSession->eventState       = 1;
            gGameSession->hideHud          = 1;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            // Keep the saved view intact until the menu scene restores it.
            _dryfieldNightSaloonGRSelectJukeboxView();
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            capRunCommand(DRYFIELD_NIGHT_SALOON_G_R_CAP_JUKEBOX, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_WAIT_CAP:
            if (capIsBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_OPEN:
            _dryfieldNightSaloonGRQueueJukebox(0);
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_SETTLE:
            task->state = task->state + 1;
            return;
        case DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_RESTORE:
            // Return to the preceding room view before releasing scripted control.
            gGameSession->eventState                                   = 0;
            gGameSession->hideHud                                      = 0;
            D_80114D08                                                 = DRYFIELD_NIGHT_SALOON_G_R_INTERACTION_REARM_UPDATES;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_dryfield_night_saloon_g_r_80188FA4.view;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            break;
        default:
            return;
    }
    taskKill(task);
}

/// NUL-terminated ASCII label for jukebox row 3, "Heaven-sent Killer" (MIDI sequence 49).
static const char _gDryfieldNightSaloonGRJukeboxRow3HeavenSentKillerLabel[] = "3. Heaven-sent Killer";
/// NUL-terminated ASCII label for jukebox row 2, "Eager For Blood" (MIDI sequence 23).
static const char _gDryfieldNightSaloonGRJukeboxRow2EagerForBloodLabel[] = "2. Eager For Blood";
/// NUL-terminated ASCII label for jukebox row 1, "Crazy King" (MIDI sequence 20).
static const char _gDryfieldNightSaloonGRJukeboxRow1CrazyKingLabel[] = "1. Crazy King";
/// NUL-terminated ASCII label for jukebox row 3, "Crawling Waste Emperor" (MIDI sequence 66).
static const char _gDryfieldNightSaloonGRJukeboxRow3CrawlingWasteEmperorLabel[] = "3. Crawling Waste Emperor";
/// NUL-terminated ASCII label for jukebox row 2, "Pick Up The Gauntlet" (MIDI sequence 60).
static const char _gDryfieldNightSaloonGRJukeboxRow2PickUpTheGauntletLabel[] = "2. Pick Up The Gauntlet";
/// NUL-terminated ASCII label for jukebox row 3, "Hunter's Moon" (MIDI sequence 74).
static const char _gDryfieldNightSaloonGRJukeboxRow3HuntersMoonLabel[] = "3. Hunter's Moon";
/// NUL-terminated ASCII label for jukebox row 2, "Quadrumanous Leader" (MIDI sequence 22).
static const char _gDryfieldNightSaloonGRJukeboxRow2QuadrumanousLeaderLabel[] = "2. Quadrumanous Leader";
/// NUL-terminated ASCII label for jukebox row 3, "Genic Reactor" (MIDI sequence 93).
static const char _gDryfieldNightSaloonGRJukeboxRow3GenicReactorLabel[] = "3. Genic Reactor";
/// NUL-terminated ASCII label for jukebox row 2, "Rebel Forces" (MIDI sequence 82).
static const char _gDryfieldNightSaloonGRJukeboxRow2RebelForcesLabel[] = "2. Rebel Forces";
/// NUL-terminated ASCII label for jukebox row 1, "Yellow Rain" (MIDI sequence 67).
static const char _gDryfieldNightSaloonGRJukeboxRow1YellowRainLabel[] = "1. Yellow Rain";
/// NUL-terminated ASCII label for jukebox row 3, "Pick Up The Gauntlet" (MIDI sequence 60).
static const char _gDryfieldNightSaloonGRJukeboxRow3PickUpTheGauntletLabel[] = "3. Pick Up The Gauntlet";
/// NUL-terminated ASCII label for jukebox row 2, "Ambush!" (MIDI sequence 21).
static const char _gDryfieldNightSaloonGRJukeboxRow2AmbushLabel[] = "2. Ambush!";
/// NUL-terminated ASCII label for jukebox row 4, "Dark Voice Of The Heart" (MIDI sequence 61).
static const char _gDryfieldNightSaloonGRJukeboxRow4DarkVoiceOfTheHeartLabel[] = "4. Dark Voice Of The Heart";
/// NUL-terminated ASCII label for jukebox row 3, "Requiem" (MIDI sequence 58).
static const char _gDryfieldNightSaloonGRJukeboxRow3RequiemLabel[] = "3. Requiem";
/// NUL-terminated ASCII label for jukebox row 2, "Rock And Fire" (MIDI sequence 45).
static const char _gDryfieldNightSaloonGRJukeboxRow2RockAndFireLabel[] = "2. Rock And Fire";
/// NUL-terminated ASCII label for jukebox row 1, "Ghost Town" (MIDI sequence 41).
static const char _gDryfieldNightSaloonGRJukeboxRow1GhostTownLabel[] = "1. Ghost Town";
/// NUL-terminated ASCII label for jukebox row 4, "Snooper" (MIDI sequence 44).
static const char _gDryfieldNightSaloonGRJukeboxRow4SnooperLabel[] = "4. Snooper";
/// NUL-terminated ASCII label for jukebox row 3, "Wild Hunt" (MIDI sequence 42).
static const char _gDryfieldNightSaloonGRJukeboxRow3WildHuntLabel[] = "3. Wild Hunt";
/// NUL-terminated ASCII label for jukebox row 2, "Lightning Operation" (MIDI sequence 36).
static const char _gDryfieldNightSaloonGRJukeboxRow2LightningOperationLabel[] = "2. Lightning Operation";
/// NUL-terminated ASCII label for jukebox row 1, "L.A. Maze" (MIDI sequence 35).
static const char _gDryfieldNightSaloonGRJukeboxRow1LaMazeLabel[] = "1. L.A. Maze";
/// NUL-terminated ASCII label for jukebox row 4, "Genic Reactor" (MIDI sequence 93).
static const char _gDryfieldNightSaloonGRJukeboxRow4GenicReactorLabel[] = "4. Genic Reactor";
/// NUL-terminated ASCII label for jukebox row 3, "Mental Agony" (MIDI sequence 89).
static const char _gDryfieldNightSaloonGRJukeboxRow3MentalAgonyLabel[] = "3. Mental Agony";
/// NUL-terminated ASCII label for jukebox row 2, "Wishwash" (MIDI sequence 59).
static const char _gDryfieldNightSaloonGRJukeboxRow2WishwashLabel[] = "2. Wishwash";
/// NUL-terminated ASCII label for jukebox row 1, "Curse Your Fate!" (MIDI sequence 31).
static const char _gDryfieldNightSaloonGRJukeboxRow1CurseYourFateLabel[] = "1. Curse Your Fate!";
/// NUL-terminated ASCII label for jukebox row 4, "Killing Field" (MIDI sequence 83).
static const char _gDryfieldNightSaloonGRJukeboxRow4KillingFieldLabel[] = "4. Killing Field";
/// NUL-terminated ASCII label for jukebox row 3, "Fool's Paradise" (MIDI sequence 78).
static const char _gDryfieldNightSaloonGRJukeboxRow3FoolsParadiseLabel[] = "3. Fool's Paradise";
/// NUL-terminated ASCII label for jukebox row 2, "Gazing into The Void" (MIDI sequence 77).
static const char _gDryfieldNightSaloonGRJukeboxRow2GazingIntoTheVoidLabel[] = "2. Gazing into The Void";
/// NUL-terminated ASCII label for jukebox row 1, "Man Made Nature" (MIDI sequence 76).
static const char _gDryfieldNightSaloonGRJukeboxRow1ManMadeNatureLabel[] = "1. Man Made Nature";
/// NUL-terminated ASCII label for jukebox row 4, "Lazing Away the Morning" (MIDI sequence 37).
static const char _gDryfieldNightSaloonGRJukeboxRow4LazingAwayTheMorningLabel[] = "4. Lazing Away the Morning";
/// NUL-terminated ASCII label for jukebox row 3, "Out Of Phase 2" (MIDI sequence 17).
static const char _gDryfieldNightSaloonGRJukeboxRow3OutOfPhase2Label[] = "3. Out Of Phase 2";
/// NUL-terminated ASCII label for jukebox row 2, "The Vagrants" (MIDI sequence 43).
static const char _gDryfieldNightSaloonGRJukeboxRow2TheVagrantsLabel[] = "2. The Vagrants";
/// NUL-terminated ASCII label for jukebox row 1, "Tower Rendezvous" (MIDI sequence 9).
static const char _gDryfieldNightSaloonGRJukeboxRow1TowerRendezvousLabel[] = "1. Tower Rendezvous";

/// The saloon's ten jukebox lists, selected by training state and saved game mode.
///
/// Slots 0..4 contain three training rows and 5..9 four regular rows. Within
/// each half, a cleared save selects gameMode 0 Replay, 1 Bounty, 2 Scavenger
/// or 3 Nightmare; clearCount zero selects slot 4 regardless of gameMode.
/// The shared row callback copies all ten pointers before reusing its stack
/// union for the text request. This constant template itself holds only lists.
/// The ordinary night-saloon context selects the regular half; the training
/// predicate requires the M.I.S.T. gallery's alternate PE resource variant.
static const JukeboxTrackLists _gJukeboxTrackLists = {
    .lists = {
        _gDryfieldNightSaloonGRJukeboxTrainingReplayTracks,
        _gDryfieldNightSaloonGRJukeboxTrainingBountyTracks,
        _gDryfieldNightSaloonGRJukeboxTrainingScavengerTracks,
        _gDryfieldNightSaloonGRJukeboxTrainingNightmareTracks,
        _gDryfieldNightSaloonGRJukeboxTrainingUnclearedTracks,
        _gDryfieldNightSaloonGRJukeboxRegularReplayTracks,
        _gDryfieldNightSaloonGRJukeboxRegularBountyTracks,
        _gDryfieldNightSaloonGRJukeboxRegularScavengerTracks,
        _gDryfieldNightSaloonGRJukeboxRegularNightmareTracks,
        _gDryfieldNightSaloonGRJukeboxRegularUnclearedTracks,
    },
};

/// NUL-terminated SELECT panel title, retaining an unread trailing 0x0D byte.
static const char _gDryfieldNightSaloonGRJukeboxSelectLabel[8] = "SELECT\0\x0D";
#include "../../shared/room_variants_saloon.inc.c"

/// Refuses every key-item use in the night saloon.
///
/// Handles message 0x13F1 without consuming the item or changing room state.
/// All arguments are ignored; the zero result selects the menu's refusal notice.
static s32 _dryfieldNightSaloonGRRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_KEY_ITEM_REFUSED = 0 };

    return DRYFIELD_NIGHT_SALOON_G_R_KEY_ITEM_REFUSED;
}

/// Handles the saloon's jukebox and progress-dependent conversation commands.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command ID: 4 holds player
/// control and spawns the jukebox scene, 8 selects the first or repeat talk
/// script and latches the first talk, and 10 starts follow-up dialogue until
/// talk progress reaches 2. Other commands do nothing. Returns zero; receiver,
/// message ID and second payload are unused. Scripts borrow loaded room data.
static s32 _dryfieldNightSaloonGRCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_SALOON_G_R_COMMAND_JUKEBOX   = 4,
        DRYFIELD_NIGHT_SALOON_G_R_COMMAND_TALK      = 8,
        DRYFIELD_NIGHT_SALOON_G_R_COMMAND_FOLLOW_UP = 10,
        DRYFIELD_NIGHT_SALOON_G_R_TALK_NEW          = 0,
        DRYFIELD_NIGHT_SALOON_G_R_TALK_STARTED      = 1,
        DRYFIELD_NIGHT_SALOON_G_R_TALK_FINISHED     = 2,
    };

    switch (commandId) {
        case DRYFIELD_NIGHT_SALOON_G_R_COMMAND_JUKEBOX:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case DRYFIELD_NIGHT_SALOON_G_R_COMMAND_TALK:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) == DRYFIELD_NIGHT_SALOON_G_R_TALK_NEW) {
                evsStartScript(D_dryfield_night_saloon_g_r_801848DC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, DRYFIELD_NIGHT_SALOON_G_R_TALK_STARTED);
            } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) == DRYFIELD_NIGHT_SALOON_G_R_TALK_STARTED) {
                evsStartScript(D_dryfield_night_saloon_g_r_80184B34, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            break;
        case DRYFIELD_NIGHT_SALOON_G_R_COMMAND_FOLLOW_UP:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) < DRYFIELD_NIGHT_SALOON_G_R_TALK_FINISHED) {
                evsStartScript(D_dryfield_night_saloon_g_r_80184D2C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            break;
    }
    return 0;
}

/// Starts the saloon encounter actors through the scene's synchronous broadcast.
///
/// Requires the current saloon context and live scene receiver. The complete
/// stack command is borrowed only during dispatch, in stage/area namespace.
static inline void _dryfieldNightSaloonGRStartEncounterActors(void)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_COMMAND_START_ENCOUNTER = 1 };
    ActorCommand startCommand;

    startCommand.context.loc.stage = gGameSession->location.loc.stage;
    startCommand.context.loc.area  = gGameSession->location.loc.area;
    startCommand.command           = DRYFIELD_NIGHT_SALOON_G_R_COMMAND_START_ENCOUNTER;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &startCommand, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Starts the saloon entry scene and releases its encounter from room triggers.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request through this call;
/// it is never retained. Action 7 starts the skippable entry scene once. In
/// variant 2 before the encounter latch, action 1 removes the barrier and cues
/// its sound; action 2 broadcasts actor command 1 in the current stage/area
/// namespace and latches the encounter. The stack command is consumed during
/// synchronous dispatch. Returns zero; the other parameters are unused.
static s32 _dryfieldNightSaloonGRRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_SALOON_G_R_ACTION_ENTRY                = 7,
        DRYFIELD_NIGHT_SALOON_G_R_ACTION_RELEASE_BARRIER      = 1,
        DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_VARIANT           = 2,
        DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_BARRIER_TRIGGER   = 13,
        DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_BARRIER_SOUND_CUE = 12,
    };

    u8 roomVariant;

    if (request->actionId == DRYFIELD_NIGHT_SALOON_G_R_ACTION_ENTRY && gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN) == 0) {
        evsStartScriptWithSkip(D_dryfield_night_saloon_g_r_80183C94, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_night_saloon_g_r_801847A4);
        gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, 1);
    }
    roomVariant = gGameSession->location.loc.variant;
    if (roomVariant == DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_VARIANT && gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE) == 0) {
        if (request->actionId == DRYFIELD_NIGHT_SALOON_G_R_ACTION_RELEASE_BARRIER) {
            worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gDryfieldNightSaloonGRActionTriggers[DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_BARRIER_TRIGGER]);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_BARRIER_SOUND_CUE), 0, 0);
        } else if (request->actionId == roomVariant) {
            _dryfieldNightSaloonGRStartEncounterActors();
            gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE, 1);
        }
    }
    return 0;
}

/// Hides the saloon's pending encounter actors through the scene's synchronous broadcast.
///
/// Requires the live saloon session and scene receiver. Command 0 in this
/// stage/area namespace selects the encounter actors' hidden state. The scene
/// forwards the complete four-byte command to placed actors with an unused
/// zero second payload; the stack record is borrowed only through dispatch.
static inline void _dryfieldNightSaloonGRHideEncounterActors(void)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_COMMAND_HIDE_ENCOUNTER_ACTORS = 0 };

    ActorCommand hideCommand;

    hideCommand.context.loc.stage = gGameSession->location.loc.stage;
    hideCommand.context.loc.area  = gGameSession->location.loc.area;
    hideCommand.command           = DRYFIELD_NIGHT_SALOON_G_R_COMMAND_HIDE_ENCOUNTER_ACTORS;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &hideCommand, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Registers the night saloon's room receiver and hides its pending encounter actors.
///
/// Requires the live room task in state 0 and the scene manager registered.
/// Variant 2 hides the encounter actors until the room action starts them,
/// unless the encounter has already started. Advances to idle state 1.
static void _dryfieldNightSaloonGRInitializeRoom(Task* task)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_VARIANT = 2 };

    task->msgTable = D_dryfield_night_saloon_g_r_8017F918;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_SALOON_G_R_ENCOUNTER_VARIANT && gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE) == 0) {
        _dryfieldNightSaloonGRHideEncounterActors();
    }
    task->state++;
}

/// Keeps the room task alive between messages without advancing its idle state.
static void _dryfieldNightSaloonGRIdleState(Task* task)
{
    char unusedFrame[0x10]; // Retains the original stack reservation; no bytes are accessed.
}

void dryfieldNightSaloonGRRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_saloon_g_r_8017D5DC;
    stateHandlers.funcs[task->state](task);
}

/// Selects the saloon room in both the live session and the live save record.
///
/// `roomId` is a valid 1-based room selector within the loaded area; no range
/// check or area reload occurs. The normal and skipped cutscene paths select 2.
static void _dryfieldNightSaloonGRSetRoom(u8 roomId)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = roomId;
    gGameSession->location.loc.room                            = roomId;
}

#include "../../shared/jukebox_row.inc.c"

/// Updates the saloon's jukebox list and loads confirmed music selections.
///
/// The UI task owns a live `UiObject` in `spawnArg2.pointer`. State 0 initializes
/// four rows (three in training mode); state 1 waits for MIDI to stop and queues
/// a load, and state 2 waits for CD completion before requesting playback.
/// `spawnArg1.value` holds the selected row (-1 initially). `status` is the
/// pending sequence byte below 241, 255 when idle, or 254 while training-mode
/// dismissal is locked. The row callback supplies only IDs in the track lists.
/// Closing during a load hides the panel until playback is requested; starting
/// playback marks the room song and skips automatic music outside training.
/// Keep the overlay, list and UI tree live until the host closes the menu.
static void _dryfieldNightSaloonGRJukeboxMenuTask(Task* task)
{
    enum {
        JUKEBOX_MENU_INITIALIZE                = 0,
        JUKEBOX_MENU_WAIT_MIDI                 = 1,
        JUKEBOX_MENU_NO_SELECTION              = -1,
        JUKEBOX_MENU_IDLE                      = 255,
        JUKEBOX_MENU_TRAINING_LOCKED           = 254,
        JUKEBOX_MENU_FIRST_NON_SEQUENCE_STATUS = 241,
        JUKEBOX_MENU_MAX_VISIBLE_ROWS          = 10,
        JUKEBOX_MENU_NORMAL_ROWS               = ARRAY_SIZE(_gDryfieldNightSaloonGRJukeboxRegularReplayTracks),
        JUKEBOX_MENU_TRAINING_ROWS             = ARRAY_SIZE(_gDryfieldNightSaloonGRJukeboxTrainingReplayTracks),
        JUKEBOX_MENU_ALL_SEQUENCES             = 0,
        JUKEBOX_MUSIC_CDF_STAGE                = 0,
        JUKEBOX_MUSIC_FILE_GROUP               = 4
    };
    u8        fileKeyBytes[4];
    u8        loadArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    UiObject* object;
    UiList*   list;
    u8        pendingSequence;
    s32       loadQueued;
    s32       loadState;
    u8        playbackRequested;

    /// Queues global music file 40100 plus the selected sequence ID.
    ///
    /// `fileKey` and `args` must be side-effect-free writable byte arrays of at
    /// least four bytes; both expressions are evaluated repeatedly. `sequence`
    /// and `hundreds` are evaluated once and narrowed to bytes; `hundreds` is 1
    /// here. Uses this function's music category constants. Byte 1 of `fileKey`
    /// is ignored; all load arguments are initialized and copied synchronously.
#define JUKEBOX_MENU_QUEUE_TRACK_LOAD(fileKey, args, sequence, hundreds) \
    {                                                                    \
        (fileKey)[3] = JUKEBOX_MUSIC_CDF_STAGE;                          \
        (fileKey)[2] = JUKEBOX_MUSIC_FILE_GROUP;                         \
        (fileKey)[0] = (sequence);                                       \
        (args)[0]    = (hundreds);                                       \
        (args)[3]    = 0;                                                \
        (args)[2]    = 0;                                                \
        (args)[1]    = CD_COMMAND_LOAD_DEFAULT;                          \
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, (fileKey), (args));           \
    }

    object = task->spawnArg2.pointer;
    list   = &_gDryfieldNightSaloonGRJukeboxList;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, _gDryfieldNightSaloonGRJukeboxSelectLabel);
    if (task->state == JUKEBOX_MENU_INITIALIZE) {
        task->spawnArg1.value = JUKEBOX_MENU_NO_SELECTION;
        if (attachmentIsTrainingMode() == 0) {
            list->itemCount = JUKEBOX_MENU_NORMAL_ROWS;
        } else {
            list->itemCount = JUKEBOX_MENU_TRAINING_ROWS;
        }
        if (list->itemCount >= JUKEBOX_MENU_MAX_VISIBLE_ROWS + 1) {
            list->visibleRowCount.unsignedValue = JUKEBOX_MENU_MAX_VISIBLE_ROWS;
        } else {
            list->visibleRowCount.unsignedValue = list->itemCount;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &object->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        object->panel.bounds.unsignedRect.x = -((s16)object->panel.bounds.unsignedRect.w / 2);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        if (attachmentIsTrainingMode() == 0) {
            task->status = JUKEBOX_MENU_IDLE;
        } else {
            task->status = JUKEBOX_MENU_TRAINING_LOCKED;
        }
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);

    // A selection fades MIDI first; playback is requested only after its CD load.
    pendingSequence = task->status;
    if (pendingSequence < JUKEBOX_MENU_FIRST_NON_SEQUENCE_STATUS) {
        loadState = task->state;
        if (loadState == JUKEBOX_MENU_WAIT_MIDI) {
            if (midiIsSequenceBusy(JUKEBOX_MENU_ALL_SEQUENCES) == 0) {
                JUKEBOX_MENU_QUEUE_TRACK_LOAD(fileKeyBytes, loadArgs, pendingSequence, loadState);
                loadQueued = 1;
            } else {
                loadQueued = 0;
            }
            if (loadQueued == 1) {
                task->state += 1;
            }
        } else {
            if (cdCmdIsIdle()) {
                sndEvtRequestMidiStart(pendingSequence, 0);
                sndEvtRequestMidiVolume(pendingSequence, (u8)gMidiMusicVolume);
                playbackRequested       = 1;
                gStageRoomMusicSequence = pendingSequence;
            } else {
                playbackRequested = 0;
            }
            if (playbackRequested == 1) {
                task->state  = JUKEBOX_MENU_WAIT_MIDI;
                task->status = JUKEBOX_MENU_IDLE;
                if (attachmentIsTrainingMode() == 0) {
                    gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
                }
                if (object->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
    }

    // Dismissal during a pending load leaves the host waiting for its completion.
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu | gPadCancelButtonMask) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            if (task->status != JUKEBOX_MENU_TRAINING_LOCKED) {
                if (task->status == JUKEBOX_MENU_IDLE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    uiStartPanelHiding(object, object->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }
#undef JUKEBOX_MENU_QUEUE_TRACK_LOAD
}

#include "../../shared/jukebox_host.inc.c"

/// Queues the saloon's jukebox host under a display-mode transition.
///
/// Borrows the room-owned descriptor until the display task spawns its host;
/// keep the overlay loaded through the menu. Both spawn payloads are zero.
/// The argument is ignored and the result is always 1, regardless of queue
/// admission. The room cutscene ignores that result.
static s32 _dryfieldNightSaloonGRQueueJukebox(s32 unusedArg)
{
    enum { DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_QUEUED_RESULT = 1 };

    displayQueueModeTask(&D_dryfield_night_saloon_g_r_80185068, 0, 0, STAGE_ENTRY_RELOAD);
    return DRYFIELD_NIGHT_SALOON_G_R_JUKEBOX_QUEUED_RESULT;
}

void dryfieldNightSaloonGRDrawGlowsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_STANDARD  = 0x200,
        DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_ALTERNATE = 0x1C0,
        DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_LARGE     = 0x300,
        DRYFIELD_NIGHT_SALOON_G_R_BEAM_RADIUS_SCALE      = 0x100,
        DRYFIELD_NIGHT_SALOON_G_R_BEAM_VIEW_MASK_INDEX   = 11,
        DRYFIELD_NIGHT_SALOON_G_R_SHAFT_VIEW_MASK_INDEX  = 12,
        DRYFIELD_NIGHT_SALOON_G_R_BEAM_START_POINT       = 13,
        DRYFIELD_NIGHT_SALOON_G_R_BEAM_END_POINT         = 12,
        DRYFIELD_NIGHT_SALOON_G_R_FINAL_FLARE_MASK_BIAS  = 7,
    };

    GfxCoord* coord;
    s32       viewMask;
    s32       pointIndex;

    coord    = task->extra.coordBody->coord;
    viewMask = 1 << gGameSession->location.loc.view;
    actorRenderComposeCoord(coord);
    // The first eleven flare centres are world positions.
    for (pointIndex = 0; pointIndex < 6; pointIndex++) {
        if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[pointIndex]) {
            _glowDrawFlare(&_gDryfieldNightSaloonGREffectPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_STANDARD);
        }
    }
    for (pointIndex = 6; pointIndex < 11; pointIndex++) {
        if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[pointIndex]) {
            _glowDrawFlare(&_gDryfieldNightSaloonGREffectPoints[pointIndex], 1, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_ALTERNATE);
        }
    }
    // Beam endpoints use the task transform rather than the flare world frame.
    if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[DRYFIELD_NIGHT_SALOON_G_R_SHAFT_VIEW_MASK_INDEX]) {
        _glowDrawTwinShafts(coord);
    }
    if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[DRYFIELD_NIGHT_SALOON_G_R_BEAM_VIEW_MASK_INDEX]) {
        _dryfieldNightSaloonGRDrawTaperedBeam(coord, &_gDryfieldNightSaloonGREffectPoints[DRYFIELD_NIGHT_SALOON_G_R_BEAM_START_POINT],
                                              &_gDryfieldNightSaloonGREffectPoints[DRYFIELD_NIGHT_SALOON_G_R_BEAM_END_POINT], DRYFIELD_NIGHT_SALOON_G_R_BEAM_RADIUS_SCALE);
    }
    // Six shaft vertices occupy points 14..19 but just one view-mask entry.
    for (pointIndex = 20; pointIndex < 23; pointIndex++) {
        if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[pointIndex - DRYFIELD_NIGHT_SALOON_G_R_FINAL_FLARE_MASK_BIAS]) {
            _glowDrawFlare(&_gDryfieldNightSaloonGREffectPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_STANDARD);
        }
    }
    for (pointIndex = 23; pointIndex < (s32)ARRAY_SIZE(_gDryfieldNightSaloonGREffectPoints); pointIndex++) {
        if (viewMask & _gDryfieldNightSaloonGREffectViewMasks[pointIndex - DRYFIELD_NIGHT_SALOON_G_R_FINAL_FLARE_MASK_BIAS]) {
            _glowDrawFlare(&_gDryfieldNightSaloonGREffectPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_LARGE);
        }
    }
}

#include "../../shared/glow_draw_flare.inc.c"

/// Binds this overlay's local shaft points for the shared drawer's inclusion.
#define GLOW_TWIN_SHAFT_POINTS _gDryfieldNightSaloonGREffectPoints
#include "../../shared/glow_draw_twin_shafts.inc.c"
#undef GLOW_TWIN_SHAFT_POINTS

/// Initializes a grey-to-black Gouraud quad for one beam-cap wedge.
///
/// Borrows one writable `POLY_G4`. Vertex 2 receives `centreIntensity`
/// (0..255) in each RGB channel; rim vertices 0, 1 and 3 are black.
/// Sets the eight-word payload length and opaque, untextured quad command.
/// The caller supplies screen coordinates, the ordering-table link and
/// additive semitransparency before submission. Two quarter-disc wedges
/// form each half-disc cap.
static inline void _dryfieldNightSaloonGRInitBeamCapWedge(POLY_G4* wedge, u8 centreIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centreIntensity, centreIntensity, centreIntensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws an additive grey beam between two saloon-local endpoints.
///
/// Borrows `coord`, `startPoint` and `endPoint`; the composed `coord->workm`
/// maps them into world space, narrowing transformed components to s16.
/// Projection through `GsWSMATRIX` accepts the second endpoint at camera Z / 4
/// depth >= 17. The first depth must be nonzero: it is used without a clamp.
/// Projection flags are not tested. Each pixel radius is the signed low
/// halfword of `radiusScale` times 64 divided by the endpoint's depth.
///
/// Opposing half-disc caps and joining bands fade grey 32/48 to a black rim
/// on alternating frames. Queues six quads and additive blend commands in the
/// current frame arena; both joining bands sort at the first endpoint's depth.
static void _dryfieldNightSaloonGRDrawTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale)
{
    GlowWorldPointPairScratch* block;
    POLY_G4*                   prim;
    s32                        angle;
    s32                        rimAngle;
    s32                        nextAngle;
    s32                        brightness;
    s32                        scaledRadius;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowWorldPointPairScratch);

    // Transform and narrow both endpoints before applying the view matrix.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(startPoint);
    gte_rtv0();
    gte_stsv(&block->worldPoint0);
    block->worldPoint0.vx += coord->workm.t[0];
    block->worldPoint0.vy += coord->workm.t[1];
    block->worldPoint0.vz += coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(endPoint);
    gte_rtv0();
    gte_stsv(&block->worldPoint1);
    block->worldPoint1.vx += coord->workm.t[0];
    block->worldPoint1.vy += coord->workm.t[1];
    block->worldPoint1.vz += coord->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&block->worldPoint1);
    gte_rtps();
    gte_stsxy(&block->sx1);
    gte_stszotz(&block->otz1);
    if (block->otz1 >= GLOW_MIN_DEPTH) {
        scaledRadius   = (s16)radiusScale * GLOW_RADIUS_SCALE;
        angle          = 0;
        brightness     = (((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) | GLOW_FLICKER_BASE_INTENSITY;
        block->radius0 = scaledRadius / block->otz0;
        block->radius1 = scaledRadius / block->otz1;
        // Join the opposing half-disc caps with two bands.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _dryfieldNightSaloonGRInitBeamCapWedge(prim, brightness);
            prim->x0  = block->sx0 + ((block->radius0 * rsin(angle)) >> GLOW_TRIG_SHIFT);
            rimAngle  = angle + GLOW_EIGHTH_TURN;
            prim->y0  = block->sy0 + ((block->radius0 * rcos(angle)) >> GLOW_TRIG_SHIFT);
            prim->x1  = block->sx0 + ((block->radius0 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1  = block->sy0 + ((block->radius0 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle = angle + GLOW_QUARTER_TURN;
            prim->x2  = block->sx0;
            prim->y2  = block->sy0;
            prim->x3  = block->sx0 + ((block->radius0 * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3  = block->sy0 + ((block->radius0 * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, brightness, brightness, brightness);
            setRGB3(prim, brightness, brightness, brightness);
            prim->x0 = block->sx0 + ((block->radius0 * rsin(angle * 2)) >> GLOW_TRIG_SHIFT);
            prim->y0 = block->sy0 + ((block->radius0 * rcos(angle * 2)) >> GLOW_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin(angle * 2)) >> GLOW_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos(angle * 2)) >> GLOW_TRIG_SHIFT);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            _dryfieldNightSaloonGRInitBeamCapWedge(prim, brightness);
            prim->x0 = block->sx1 + ((block->radius1 * rsin(GLOW_FULL_TURN - angle)) >> GLOW_TRIG_SHIFT);
            prim->y0 = block->sy1 + ((block->radius1 * rcos(GLOW_FULL_TURN - angle)) >> GLOW_TRIG_SHIFT);
            prim->x1 = block->sx1 + ((block->radius1 * rsin((GLOW_FULL_TURN - GLOW_EIGHTH_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            prim->y1 = block->sy1 + ((block->radius1 * rcos((GLOW_FULL_TURN - GLOW_EIGHTH_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->radius1 * rsin((GLOW_FULL_TURN - GLOW_QUARTER_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            prim->y3 = block->sy1 + ((block->radius1 * rcos((GLOW_FULL_TURN - GLOW_QUARTER_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz1);
        } while (angle < GLOW_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowWorldPointPairScratch);
}
