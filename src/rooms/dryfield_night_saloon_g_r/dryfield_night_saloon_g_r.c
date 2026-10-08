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

extern WorldCollisionTrigger D_dryfield_night_saloon_g_r_801887DC[21];

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

/// The jukebox's track lists, one per game mode, each a run of track id and
/// name pairs.
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F0C[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F24[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F3C[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F54[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F6C[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184F84[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184FA4[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184FC4[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80184FE4[];
extern JukeboxTrack D_dryfield_night_saloon_g_r_80185004[];

/// The jukebox menu's title, "SELECT". A stray 0x0D byte follows its
/// terminator, so the block stays in assembly.
static const char D_dryfield_night_saloon_g_r_8017D898[];

/// The jukebox's track list.
extern UiList D_dryfield_night_saloon_g_r_80185028;

/// Descriptor of the jukebox menu panel, whose task is
/// `_dryfieldNightSaloonGRJukeboxMenuTask`.
extern UiObjectDesc gJukeboxPanelDesc;

/// Descriptor of the jukebox task `jukeboxHostTask`.
extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

/// The room's effect positions in the model's local space. The frame hook
/// draws a quad at each of 0-10 and 20-27; 12 and 13 are the two ends
/// `_dryfieldNightSaloonGRDrawTaperedBeam` is handed; 14-19 are the two
/// light shafts of `_glowDrawTwinShafts`.

/// One view bitmask per effect, tested against `1 << view`. Entries 0-10 gate
/// positions 0-10, entries 11 and 12 the two helper effects, and entries 13-20
/// gate positions 20-27.
extern s16 D_dryfield_night_saloon_g_r_80185154[];

static void _dryfieldNightSaloonGRInitializeRoom(Task* task);
static void _dryfieldNightSaloonGRIdleState(Task* task);
static s32  func_dryfield_night_saloon_g_r_8017E698(s32 arg0);
static void _dryfieldNightSaloonGRDrawTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale);

static void _dryfieldNightSaloonGRJukeboxMenuTask(Task* task);

extern WorldCoordRoomLights D_dryfield_night_saloon_g_r_80188304[1];

extern WorldCollisionGrid     D_dryfield_night_saloon_g_r_80185B50[1];
extern WorldCollisionOccluder D_dryfield_night_saloon_g_r_80188E18[2];
extern WorldCollisionTrigger  D_dryfield_night_saloon_g_r_8018831C[16];
extern WorldCollisionTrigger  D_dryfield_night_saloon_g_r_801887DC[21];

extern SpriteBatch  D_dryfield_night_saloon_g_r_80185D48[2];
extern SpriteBatch  D_dryfield_night_saloon_g_r_80185EAC[3];
extern SpriteBatch  D_dryfield_night_saloon_g_r_80185FDC[4];
extern SpriteBatch  D_dryfield_night_saloon_g_r_80186574[6];
extern SpriteBatch  D_dryfield_night_saloon_g_r_80186A90[5];
extern SpriteBatch  D_dryfield_night_saloon_g_r_801871D4[5];
extern SpriteBatch  D_dryfield_night_saloon_g_r_801873C8[5];
extern SpriteBatch  D_dryfield_night_saloon_g_r_801873F0[2];
extern SpriteBatch  D_dryfield_night_saloon_g_r_80187504[5];
extern SpriteSource D_dryfield_night_saloon_g_r_80185D58[17];
extern SpriteSource D_dryfield_night_saloon_g_r_80185EC4[14];
extern SpriteSource D_dryfield_night_saloon_g_r_80185FFC[70];
extern SpriteSource D_dryfield_night_saloon_g_r_801865A4[63];
extern SpriteSource D_dryfield_night_saloon_g_r_80186AB8[91];
extern SpriteSource D_dryfield_night_saloon_g_r_801871FC[23];
extern SpriteSource D_dryfield_night_saloon_g_r_80187400[13];

extern AnimationPlayRequest     D_dryfield_night_saloon_g_r_80183968;
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
extern const char               D_dryfield_night_saloon_g_r_8017D600[22];
extern const char               D_dryfield_night_saloon_g_r_8017D618[19];
extern const char               D_dryfield_night_saloon_g_r_8017D62C[14];
extern const char               D_dryfield_night_saloon_g_r_8017D63C[26];
extern const char               D_dryfield_night_saloon_g_r_8017D658[24];
extern const char               D_dryfield_night_saloon_g_r_8017D670[17];
extern const char               D_dryfield_night_saloon_g_r_8017D684[23];
extern const char               D_dryfield_night_saloon_g_r_8017D69C[17];
extern const char               D_dryfield_night_saloon_g_r_8017D6B0[16];
extern const char               D_dryfield_night_saloon_g_r_8017D6C0[15];
extern const char               D_dryfield_night_saloon_g_r_8017D6D0[24];
extern const char               D_dryfield_night_saloon_g_r_8017D6E8[11];
extern const char               D_dryfield_night_saloon_g_r_8017D6F4[27];
extern const char               D_dryfield_night_saloon_g_r_8017D710[11];
extern const char               D_dryfield_night_saloon_g_r_8017D71C[17];
extern const char               D_dryfield_night_saloon_g_r_8017D730[14];
extern const char               D_dryfield_night_saloon_g_r_8017D740[11];
extern const char               D_dryfield_night_saloon_g_r_8017D74C[13];
extern const char               D_dryfield_night_saloon_g_r_8017D75C[23];
extern const char               D_dryfield_night_saloon_g_r_8017D774[13];
extern const char               D_dryfield_night_saloon_g_r_8017D784[17];
extern const char               D_dryfield_night_saloon_g_r_8017D798[16];
extern const char               D_dryfield_night_saloon_g_r_8017D7A8[12];
extern const char               D_dryfield_night_saloon_g_r_8017D7B4[20];
extern const char               D_dryfield_night_saloon_g_r_8017D7C8[17];
extern const char               D_dryfield_night_saloon_g_r_8017D7DC[19];
extern const char               D_dryfield_night_saloon_g_r_8017D7F0[24];
extern const char               D_dryfield_night_saloon_g_r_8017D808[19];
extern const char               D_dryfield_night_saloon_g_r_8017D81C[27];
extern const char               D_dryfield_night_saloon_g_r_8017D838[18];
extern const char               D_dryfield_night_saloon_g_r_8017D84C[16];
extern const char               D_dryfield_night_saloon_g_r_8017D85C[20];
static void                     _dryfieldNightSaloonGRSetRoom(u8 roomId);

static s32 _dryfieldNightSaloonGRRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
s32        func_dryfield_night_saloon_g_r_8017DD84(Task*, s32, s32, s32);
s32        func_dryfield_night_saloon_g_r_8017DE68(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void       func_dryfield_night_saloon_g_r_8017DB74(Task*);

enum {
    DRYFIELD_NIGHT_SALOON_G_R_MESSAGE_USE_KEY_ITEM = 0x13F1,
    DRYFIELD_NIGHT_SALOON_G_R_ROOM_AFTER_CUTSCENE  = 2,
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_saloon_g_r_8017F918[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantSaloonMsg },
    { DRYFIELD_NIGHT_SALOON_G_R_MESSAGE_USE_KEY_ITEM, _dryfieldNightSaloonGRRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_saloon_g_r_8017DE68 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_saloon_g_r_8017DD84 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_saloon_g_r_8017F940[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_night_saloon_g_r_8017DB74, { .value = 0 } },
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

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183968 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

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

AnimationPlayRequest D_dryfield_night_saloon_g_r_80183B64 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53120006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_saloon_g_r_80183968 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5312000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_dryfield_night_saloon_g_r_80183B64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F0C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 23, D_dryfield_night_saloon_g_r_8017D618 },
    { 49, D_dryfield_night_saloon_g_r_8017D600 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F24[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 60, D_dryfield_night_saloon_g_r_8017D658 },
    { 66, D_dryfield_night_saloon_g_r_8017D63C },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F3C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 22, D_dryfield_night_saloon_g_r_8017D684 },
    { 74, D_dryfield_night_saloon_g_r_8017D670 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F54[3] = {
    { 67, D_dryfield_night_saloon_g_r_8017D6C0 },
    { 82, D_dryfield_night_saloon_g_r_8017D6B0 },
    { 93, D_dryfield_night_saloon_g_r_8017D69C },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F6C[3] = {
    { 20, D_dryfield_night_saloon_g_r_8017D62C },
    { 21, D_dryfield_night_saloon_g_r_8017D6E8 },
    { 60, D_dryfield_night_saloon_g_r_8017D6D0 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184F84[4] = {
    { 41, D_dryfield_night_saloon_g_r_8017D730 },
    { 45, D_dryfield_night_saloon_g_r_8017D71C },
    { 58, D_dryfield_night_saloon_g_r_8017D710 },
    { 61, D_dryfield_night_saloon_g_r_8017D6F4 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184FA4[4] = {
    { 35, D_dryfield_night_saloon_g_r_8017D774 },
    { 36, D_dryfield_night_saloon_g_r_8017D75C },
    { 42, D_dryfield_night_saloon_g_r_8017D74C },
    { 44, D_dryfield_night_saloon_g_r_8017D740 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184FC4[4] = {
    { 31, D_dryfield_night_saloon_g_r_8017D7B4 },
    { 59, D_dryfield_night_saloon_g_r_8017D7A8 },
    { 89, D_dryfield_night_saloon_g_r_8017D798 },
    { 93, D_dryfield_night_saloon_g_r_8017D784 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80184FE4[4] = {
    { 76, D_dryfield_night_saloon_g_r_8017D808 },
    { 77, D_dryfield_night_saloon_g_r_8017D7F0 },
    { 78, D_dryfield_night_saloon_g_r_8017D7DC },
    { 83, D_dryfield_night_saloon_g_r_8017D7C8 },
};

JukeboxTrack D_dryfield_night_saloon_g_r_80185004[4] = {
    { 9, D_dryfield_night_saloon_g_r_8017D85C },
    { 43, D_dryfield_night_saloon_g_r_8017D84C },
    { 17, D_dryfield_night_saloon_g_r_8017D838 },
    { 37, D_dryfield_night_saloon_g_r_8017D81C },
};

UiListRowCallback D_dryfield_night_saloon_g_r_80185024[1] = {
    _jukeboxDrawRow,
};

UiList D_dryfield_night_saloon_g_r_80185028 = { D_dryfield_night_saloon_g_r_80185024, 1, { .unsignedValue = 1 }, 0, 17, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc gJukeboxPanelDesc = { USER_INTERFACE_PANEL_TITLE_STYLE, { -112, -64, 224, 128 }, 48, 0, TASK_BODY_NONE, 192, _dryfieldNightSaloonGRJukeboxMenuTask, 0 };

TaskDesc D_dryfield_night_saloon_g_r_80185068 = { { { TASK_BODY_NONE, 192 } }, jukeboxHostTask, { .value = 0 } };

SVECTOR gSaloonLightPoints[28] = {
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

s16 D_dryfield_night_saloon_g_r_80185154[22] = {
    2176,
    2060,
    4,
    1032,
    560,
    560,
    656,
    2704,
    2688,
    2176,
    2176,
    656,
    560,
    1024,
    1024,
    1024,
    1024,
    1024,
    1024,
    0,
    512,
    -3540,
};

WorldCoordRoomLighting D_dryfield_night_saloon_g_r_80185180[2] = {
    { D_dryfield_night_saloon_g_r_80188304, NULL },
    { D_dryfield_night_saloon_g_r_80188304, NULL },
};

WorldCollisionRoomResources D_dryfield_night_saloon_g_r_80185190[2] = {
    { D_dryfield_night_saloon_g_r_80185B50, D_dryfield_night_saloon_g_r_8018831C, D_dryfield_night_saloon_g_r_801887DC, D_dryfield_night_saloon_g_r_80188E18 },
    { D_dryfield_night_saloon_g_r_80185B50, D_dryfield_night_saloon_g_r_8018831C, D_dryfield_night_saloon_g_r_801887DC, D_dryfield_night_saloon_g_r_80188E18 },
};

u8* D_dryfield_night_saloon_g_r_801851B0[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_dryfield_night_saloon_g_r_801851B8[2] = { 13, 13 };

DirectionWarpEntry D_dryfield_night_saloon_g_r_801851BC[2] = {
    { { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, 0x53120002, 0x53120001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, 0x53120004, 0x53120003, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 478 },
};

static SVECTOR _gDryfieldNightSaloonGRCollision08590Normals[9] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_normals.inc"
};

static SVECTOR _gDryfieldNightSaloonGRCollision08590Verts[116] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightSaloonGRCollision08590Faces[61] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_faces.inc"
};

static s16 _gDryfieldNightSaloonGRCollision08590Cells[280] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightSaloonGRCollision08590Cells[i])
static s16* _gDryfieldNightSaloonGRCollision08590Table[12] = {
#include "assets/dryfield_night_saloon_g_r_collision_08590_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_saloon_g_r_80185B50[1] = {
    { NULL, _gDryfieldNightSaloonGRCollision08590Normals, _gDryfieldNightSaloonGRCollision08590Verts, _gDryfieldNightSaloonGRCollision08590Faces, _gDryfieldNightSaloonGRCollision08590Table, 4500, 5400, 3, 4, 4000, 61 },
};

ViewCamera D_dryfield_night_saloon_g_r_80185B74[13] = {
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

SpriteBatch D_dryfield_night_saloon_g_r_80185D48[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80185D58[17] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, 48, 550, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 88, 550, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 104, 500, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 16, 96, 500, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 16, 64, 525, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 72, 104, 500, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, 80, 500, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -8, 850, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 850, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 40, 875, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1050, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1050, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 8, 1075, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -8, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 1075, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -16, 1075, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80185EAC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80185EC4[14] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 1200, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, 8, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, 16, 975, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -32, -16, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -32, 16, 975, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 48, 112, 719, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 16, 104, 730, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, 104, 516, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, 96, 522, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 96, 521, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 529, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -128, 88, 529, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -136, 96, 532, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, 112, 527, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80185FDC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80185FFC[70] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -112, 1250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -112, 1250, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -56, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 1325, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -32, 1325, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -32, -32, 1375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, -16, 1325, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1375, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 1325, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1325, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 0, 1375, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -16, 1375, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, 16, 1375, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, -32, 1350, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -16, 1325, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 1325, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -16, 1350, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 0, 1325, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 16, 1250, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -152, 24, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 40 } }, -8, 80, 705, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -8, 48, 798, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 8, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 32 } }, -24, 16, 1000, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, 48, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 64, 925, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 32, 925, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -112, -120, 1775, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -104, -112, 1843, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -96, -72, 1758, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 48 } }, -80, -80, 1981, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 40 } }, -80, -120, 1847, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -8, -120, 1873, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -8, -96, 2291, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, -120, 1855, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -96, 1921, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, -72, 2025, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, -48, 2105, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -40, -48, 1930, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -72, 1985, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -96, 1900, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -32, 1833, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -48, 1615, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 88, -64, 1862, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, -64, 2213, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 80, -40, 1611, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 96, -64, 1775, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -40, 1588, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 96, -24, 1724, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 104, -16, 1623, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -40, 1414, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -64, 1727, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -56, 1608, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -32, 1377, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 112, -8, 1540, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, 0, 1455, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -24, 1509, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -48, 1480, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -48, 1424, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 128, -16, 1229, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 8, 1378, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, 8, 1327, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -16, 1213, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -40, 1371, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, -40, 1323, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 144, -8, 1292, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 144, 16, 1059, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 152, 24, 997, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 0, 1065, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 152, -32, 1251, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80186574[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 7, 0, 0, { 1, 0 } },
    { 27, 14, 0, 0, { 2, 0 } },
    { 41, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_801865A4[63] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 250, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 16, -24, 250, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -88, 250, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 128, -48, 250, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 32, 250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -32, 40, 250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -48, 1143, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 8, 1253, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -40, 1150, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -8, 1175, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -88, 934, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -40, 999, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, 8, 932, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -40, 1044, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, 8, 1011, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 8, 1116, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -40, 1208, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -40, 1046, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -40, 1090, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -24, 1183, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -80, -120, 940, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -128, -120, 917, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, -88, 932, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -88, 921, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -112, -72, 1008, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, -88, 1024, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -64, -72, 1083, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -88, 1250, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -72, 1312, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -56, 1325, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -40, 1375, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -24, 1500, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -16, 1500, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -8, 1500, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -88, 1250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -72, 1250, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -56, 1312, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -40, 1325, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -24, 1337, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -16, 1375, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, -8, 1375, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 0, 1500, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -88, 1250, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1250, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -56, 1250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -40, 1325, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 1375, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -8, 1375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -96, 88, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 80, 500, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 72, 500, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, 64, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 24, 56, 500, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 56, 48, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 40, 500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 32, 500, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 40, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 80, 88, 500, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 500, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80186A90[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 42, 0, 0, { 2, 0 } },
    { 52, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80186AB8[91] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 375, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 72, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, 80, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 80, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 80, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 72, 250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 250, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 80, 250, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 250, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 156, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 742, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -40, 750, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 689, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 843, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -64, 547, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -64, 541, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -64, 589, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 598, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -40, 645, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -40, 723, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -40, 686, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 751, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -40, 576, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 56, 549, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 24, 460, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 384, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 0, 386, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -8, 354, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -24, 301, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -40, 295, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 293, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -64, 276, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 261, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -104, 247, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 299, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -120, 258, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -104, 267, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, -120, 325, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -120, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -24, 641, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -24, 748, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -32, 698, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -48, 649, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -64, 866, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -88, 919, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -120, 827, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, -120, 492, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -88, 564, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -96, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -120, 423, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -120, 453, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -120, 490, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 246, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -48, 259, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -32, 271, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -16, 272, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -8, 307, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 8, 328, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -16, 239, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 252, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 32, 324, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 260, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 64, 262, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 625, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 615, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -24, 625, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -48, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 625, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -80, 625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, -96, 625, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 731, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -64, 625, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -72, 625, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 358, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 32, 386, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 48, 420, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 448, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 492, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 64, 613, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 290, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_801871D4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 13, 0, 0, { 2, 0 } },
    { 32, 59, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_801871FC[23] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 16, 625, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -8, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 72, 40, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 8, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 56, 500, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, -8, 500, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, 40, 500, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 0, 40, 500, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, -16, 500, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -32, 16, 625, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -32, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -64, -40, 975, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, 0, 975, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -128, -32, 1175, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -40, 1125, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -8, 1125, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -80, 1550, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, -32, 1550, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -48, -80, 1550, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -32, 1550, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, 8, 950, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 8, 987, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 40, 1050, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_801873C8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_saloon_g_r_801873F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80187400[13] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 72, 525, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 88, 500, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 64, 525, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 80, 500, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 525, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 500, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 0, 350, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, -24, 362, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -56, 375, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 387, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -88, 387, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80187504[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 1, 0, 0, { 2, 0 } },
    { 8, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_8018752C[119] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 130, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 88, 130, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 72, 130, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 88, 130, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 88, 135, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 80, 149, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 137, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 80, 135, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 32, 152, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 144, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 56, 141, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 40, 132, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 56, 132, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 40, 129, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 129, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 56, 139, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -8, 140, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 144, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 140, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 142, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 24, 140, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 737, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 140, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 40, 140, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 40, 142, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 139, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 72, 140, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 140, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 72, 141, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 88, 141, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 88, 142, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 154, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 64, 150, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 151, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 292, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 159, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 72, 156, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 56, 236, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 32, 152, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 48, 146, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 147, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 148, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 96, 148, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 72, 191, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 152, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 48, 146, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 64, 146, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 80, 148, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 147, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 150, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 151, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 169, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 152, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 24, 152, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 152, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 48, 156, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 64, 157, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 148, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 64, 146, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 146, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 149, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 149, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 40, 149, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 56, 144, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 72, 144, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 926, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 32, 1258, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 48, 149, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 64, 146, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 56, 153, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 155, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, 40, 300, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, 40, 300, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -16, 40, 300, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, 40, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 48, 40, 300, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 40, 250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 40, 250, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 129, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 96, 130, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 112, 131, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 80, 132, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 96, 133, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 133, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 141, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 96, 141, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 112, 141, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 80, 183, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 96, 156, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 112, 144, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 139, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 140, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 144, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 80, 150, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 96, 150, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 112, 144, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 152, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 152, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 112, 144, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 153, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 96, 154, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 112, 144, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 80, 147, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 96, 148, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 112, 144, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 80, 183, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 96, 159, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 112, 141, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 80, 150, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 96, 151, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 145, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 80, 145, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 96, 146, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 112, 145, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 148, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 96, 150, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 112, 145, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 80, 177, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 104, 150, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80187E78[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 71, 0, 0, { 1, 0 } },
    { 71, 48, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_saloon_g_r_80187E98[12] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -96, 56, 612, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 56, 800, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, 8, 800, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, 0, 800, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, 56, 800, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 8, 800, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 56, 800, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 8, 800, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 56, 800, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 800, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 40, 800, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 72, 800, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_saloon_g_r_80187F88[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_saloon_g_r_80187FA8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_saloon_g_r_80187FB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_saloon_g_r_80187FC8[13] = {
    { { .empty = D_dryfield_night_saloon_g_r_80185D48 }, D_dryfield_night_saloon_g_r_80185D48, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185D58 }, D_dryfield_night_saloon_g_r_80185EAC, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185EC4 }, D_dryfield_night_saloon_g_r_80185FDC, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80185FFC }, D_dryfield_night_saloon_g_r_80186574, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_801865A4 }, D_dryfield_night_saloon_g_r_80186A90, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80186AB8 }, D_dryfield_night_saloon_g_r_801871D4, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_801871FC }, D_dryfield_night_saloon_g_r_801873C8, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_801873F0 }, D_dryfield_night_saloon_g_r_801873F0, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80187400 }, D_dryfield_night_saloon_g_r_80187504, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_8018752C }, D_dryfield_night_saloon_g_r_80187E78, NULL },
    { { .elements = D_dryfield_night_saloon_g_r_80187E98 }, D_dryfield_night_saloon_g_r_80187F88, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_80187FA8 }, D_dryfield_night_saloon_g_r_80187FA8, NULL },
    { { .empty = D_dryfield_night_saloon_g_r_80187FB8 }, D_dryfield_night_saloon_g_r_80187FB8, NULL },
};

WorldCoordPointLight D_dryfield_night_saloon_g_r_80188064[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1392, 1556, 1720 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 940, -1500, 5040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3768, 3522, 2703 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1850, -1500, -3210 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 2621 }, { 0, 0 } }, 1500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3480, -1500, -3530 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3932, 4096 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3780, -1600, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3112, 2457 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4750, -1400, 4320 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2490, -1000, 3100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3276, 2375 }, { 0, 0 } }, 1000, 3000 },
};

WorldCoordRoomLights D_dryfield_night_saloon_g_r_80188304[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_saloon_g_r_80188064), D_dryfield_night_saloon_g_r_80188064, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_saloon_g_r_8018831C[16] = {
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

WorldCollisionTrigger D_dryfield_night_saloon_g_r_801887DC[21] = {
    { NULL, NULL, NULL, { 2464, -48, -4720, 0 }, { { 624, 0, -272, 0 }, { 624, 0, 272, 0 }, { -624, 0, -272, 0 }, { -624, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4480, -50, 4368, 0 }, { { -448, 0, -624, 0 }, { 448, 0, -624, 0 }, { -448, 0, 624, 0 }, { 448, 0, 624, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3232, -64, 3392, 0 }, { { 624, 0, -880, 0 }, { 624, 0, 880, 0 }, { -624, 0, -880, 0 }, { -624, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -32, -64, 1728, 0 }, { { 2032, 0, -1392, 0 }, { 2032, 0, 1392, 0 }, { -2032, 0, -1392, 0 }, { -2032, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4076, 0, 401, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4048, -64, -4672, 0 }, { { 896, 0, -848, 0 }, { 896, 0, 848, 0 }, { -896, 0, -848, 0 }, { -896, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { -1380, 0, 3856, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6432, 0 }, { { 464, 0, -272, 0 }, { 464, 0, 272, 0 }, { -464, 0, -272, 0 }, { -464, 0, 272, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1472, -64, -1152, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, 288, 0 }, { { 1776, 0, -272, 0 }, { 1776, 0, 272, 0 }, { -1776, 0, -272, 0 }, { -1776, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1796, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4560, -64, 3456, 0 }, { { 1088, 0, -272, 0 }, { 1088, 0, 272, 0 }, { -1088, 0, -272, 0 }, { -1088, 0, 272, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3424, -64, 4384, 0 }, { { 384, 0, -976, 0 }, { 384, 0, 976, 0 }, { -384, 0, -976, 0 }, { -384, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4688, -64, 5040, 0 }, { { 928, 0, -320, 0 }, { 928, 0, 320, 0 }, { -928, 0, -320, 0 }, { -928, 0, 320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 981, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2080, -64, 1328, 0 }, { { 880, 0, -1056, 0 }, { 880, 0, 1056, 0 }, { -880, 0, -1056, 0 }, { -880, 0, 1056, 0 } }, { 0, 4104, 0, 0 }, { -4092, 0, -202, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3968, -64, 1408, 0 }, { { 704, 0, -991, 0 }, { 704, 0, 992, 0 }, { -704, 0, -991, 0 }, { -704, 0, 992, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, 600, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 224, -64, 4176, 0 }, { { 2768, 0, -1024, 0 }, { 2768, 0, 1024, 0 }, { -2768, 0, -1024, 0 }, { -2768, 0, 1024, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 2941, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, 5648, 0 }, { { 1232, 0, -464, 0 }, { 1232, 0, 464, 0 }, { -1232, 0, -464, 0 }, { -1232, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
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

AreaResource D_dryfield_night_saloon_g_r_80188EA8[2] = {
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04000_D0C6E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_saloon_g_r_80188EC0[3] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { 40, 40, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_204000_80156500 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_saloon_g_r_80188EE4[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017BE58, D_dryfield_night_saloon_g_r_80188E90 },
    { D_map_dryfield_full_8017BE78, D_dryfield_night_saloon_g_r_80188EA8 },
    { D_map_dryfield_full_8017BEA8, D_dryfield_night_saloon_g_r_80188EC0 },
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

WorldCollisionFootstepSounds D_dryfield_night_saloon_g_r_80188F4C = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_night_saloon_g_r_80188F58 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_night_saloon_g_r_80188F64[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_saloon_g_r_80188F6C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_saloon_g_r_80188F4C },
};

WorldCollisionSurfaceProperties D_dryfield_night_saloon_g_r_80188F74[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_saloon_g_r_80188F58 },
};

WorldCollisionSurfaceProperties D_dryfield_night_saloon_g_r_80188F7C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_saloon_g_r_80188F58 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_saloon_g_r_80188F84[8] = {
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F6C,
    D_dryfield_night_saloon_g_r_80188F74,
    D_dryfield_night_saloon_g_r_80188F7C,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
    D_dryfield_night_saloon_g_r_80188F64,
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

/// Room cutscene task: case 0 saves the view slot, forces `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view`
/// to 0xC, raises the script halt flags and starts cap command 0x13; the
/// following states wait for the cap to go idle, then start the jukebox task,
/// and case 4 restores the saved view slot and kills the task.
void func_dryfield_night_saloon_g_r_8017DB74(Task* task)
{
    McSaveData* save;
    u8          view;

    switch (task->state) {
        case 0:
            gGameSession->eventState                  = 1;
            gGameSession->hideHud                     = 1;
            gSceneCombatState.actorControl            = SCENE_COMBAT_ACTORS_HIDDEN;
            save                                      = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            view                                      = save->state.location.loc.view;
            save->state.location.loc.view             = 0xC;
            D_dryfield_night_saloon_g_r_80188FA4.view = view;
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            capRunCommand(0x13, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            return;
        case 1:
            if (capIsBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            func_dryfield_night_saloon_g_r_8017E698(0);
            task->state = task->state + 1;
            return;
        case 3:
            task->state = task->state + 1;
            return;
        case 4:
            gGameSession->eventState                                   = 0;
            gGameSession->hideHud                                      = 0;
            D_80114D08                                                 = 0xA;
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

/// Numbered names the room lists. Nothing in the code refers to them: the
/// room's data holds them in records that pair each with a small number.
const char D_dryfield_night_saloon_g_r_8017D600[] = "3. Heaven-sent Killer";
const char D_dryfield_night_saloon_g_r_8017D618[] = "2. Eager For Blood";
const char D_dryfield_night_saloon_g_r_8017D62C[] = "1. Crazy King";
const char D_dryfield_night_saloon_g_r_8017D63C[] = "3. Crawling Waste Emperor";
const char D_dryfield_night_saloon_g_r_8017D658[] = "2. Pick Up The Gauntlet";
const char D_dryfield_night_saloon_g_r_8017D670[] = "3. Hunter's Moon";
const char D_dryfield_night_saloon_g_r_8017D684[] = "2. Quadrumanous Leader";
const char D_dryfield_night_saloon_g_r_8017D69C[] = "3. Genic Reactor";
const char D_dryfield_night_saloon_g_r_8017D6B0[] = "2. Rebel Forces";
const char D_dryfield_night_saloon_g_r_8017D6C0[] = "1. Yellow Rain";
const char D_dryfield_night_saloon_g_r_8017D6D0[] = "3. Pick Up The Gauntlet";
const char D_dryfield_night_saloon_g_r_8017D6E8[] = "2. Ambush!";
const char D_dryfield_night_saloon_g_r_8017D6F4[] = "4. Dark Voice Of The Heart";
const char D_dryfield_night_saloon_g_r_8017D710[] = "3. Requiem";
const char D_dryfield_night_saloon_g_r_8017D71C[] = "2. Rock And Fire";
const char D_dryfield_night_saloon_g_r_8017D730[] = "1. Ghost Town";
const char D_dryfield_night_saloon_g_r_8017D740[] = "4. Snooper";
const char D_dryfield_night_saloon_g_r_8017D74C[] = "3. Wild Hunt";
const char D_dryfield_night_saloon_g_r_8017D75C[] = "2. Lightning Operation";
const char D_dryfield_night_saloon_g_r_8017D774[] = "1. L.A. Maze";
const char D_dryfield_night_saloon_g_r_8017D784[] = "4. Genic Reactor";
const char D_dryfield_night_saloon_g_r_8017D798[] = "3. Mental Agony";
const char D_dryfield_night_saloon_g_r_8017D7A8[] = "2. Wishwash";
const char D_dryfield_night_saloon_g_r_8017D7B4[] = "1. Curse Your Fate!";
const char D_dryfield_night_saloon_g_r_8017D7C8[] = "4. Killing Field";
const char D_dryfield_night_saloon_g_r_8017D7DC[] = "3. Fool's Paradise";
const char D_dryfield_night_saloon_g_r_8017D7F0[] = "2. Gazing into The Void";
const char D_dryfield_night_saloon_g_r_8017D808[] = "1. Man Made Nature";
const char D_dryfield_night_saloon_g_r_8017D81C[] = "4. Lazing Away the Morning";
const char D_dryfield_night_saloon_g_r_8017D838[] = "3. Out Of Phase 2";
const char D_dryfield_night_saloon_g_r_8017D84C[] = "2. The Vagrants";
const char D_dryfield_night_saloon_g_r_8017D85C[] = "1. Tower Rendezvous";

/// The jukebox's ten track lists: one per game mode, with list 4 standing in
/// before the first clear, and the second five used outside the debug attach
/// room.
static const JukeboxTrackLists _gJukeboxTrackLists = {
    {
        D_dryfield_night_saloon_g_r_80184F0C,
        D_dryfield_night_saloon_g_r_80184F24,
        D_dryfield_night_saloon_g_r_80184F3C,
        D_dryfield_night_saloon_g_r_80184F54,
        D_dryfield_night_saloon_g_r_80184F6C,
        D_dryfield_night_saloon_g_r_80184F84,
        D_dryfield_night_saloon_g_r_80184FA4,
        D_dryfield_night_saloon_g_r_80184FC4,
        D_dryfield_night_saloon_g_r_80184FE4,
        D_dryfield_night_saloon_g_r_80185004,
    },
};

/// "SELECT", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_night_saloon_g_r_8017D898[8] = "SELECT\0\x0D";
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

s32 func_dryfield_night_saloon_g_r_8017DD84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case 8:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) == 0) {
                evsStartScript(D_dryfield_night_saloon_g_r_801848DC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS, 1);
            } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) == 1) {
                evsStartScript(D_dryfield_night_saloon_g_r_80184B34, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            break;
        case 10:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_TALK_PROGRESS) < 2) {
                evsStartScript(D_dryfield_night_saloon_g_r_80184D2C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            break;
    }
    return 0;
}

/// Handler for this room's script entry 0x13EF, whose `DirectionActionRequest` payload
/// arrives as `request`. `actionId == 7` plays the room's first-visit cutscene
/// once (nibble 0x59). Then, in session phase 2 with nibble 0xB0 still clear,
/// `actionId == 1` unlinks the room's 4A object and queues sound 0x5312000C,
/// while action 2 announces the visit to the
/// slot-4 task with message 0x7DA carrying the session's two id bytes and a
/// non-zero action halfword, and sets nibble 0xB0. Always returns 0.
s32 func_dryfield_night_saloon_g_r_8017DE68(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    ActorCommand msg;
    u8           temp_s0;

    if (((const DirectionActionRequest*)firstArg)->actionId == 7 && gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN) == 0) {
        evsStartScriptWithSkip(D_dryfield_night_saloon_g_r_80183C94, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_night_saloon_g_r_801847A4);
        gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_CUTSCENE_SEEN, 1);
    }
    temp_s0 = gGameSession->location.loc.variant;
    if (temp_s0 == 2 && gameFlagGetNibble(GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE) == 0) {
        if (((const DirectionActionRequest*)firstArg)->actionId == 1) {
            worldCollisionUnlinkTrigger(0, &D_dryfield_night_saloon_g_r_801887DC[13]);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, 0x0C), 0, 0);
        } else if (((const DirectionActionRequest*)firstArg)->actionId == temp_s0) {
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            gameFlagSetNibble(GAME_FLAG_NIGHT_SALOON_ENCOUNTER_DONE, 1);
        }
    }
    return 0;
}

/// Hides the saloon's pending encounter actors through the scene's synchronous broadcast.
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
        JUKEBOX_MENU_NORMAL_ROWS               = ARRAY_SIZE(D_dryfield_night_saloon_g_r_80184F84),
        JUKEBOX_MENU_TRAINING_ROWS             = ARRAY_SIZE(D_dryfield_night_saloon_g_r_80184F0C),
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
    list   = &D_dryfield_night_saloon_g_r_80185028;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&object->panel, D_dryfield_night_saloon_g_r_8017D898);
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
                sndEvtRequestMidiVolume(pendingSequence, (u8)D_8007A396);
                playbackRequested = 1;
                gStageRoomSong    = pendingSequence;
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
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu | Pad_MaskCancel) != 0) {
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

/// Starts the jukebox task and reports success. Its argument is unused;
/// `func_dryfield_night_saloon_g_r_8017DB74` (state 2) still passes one.
static s32 func_dryfield_night_saloon_g_r_8017E698(s32 arg0)
{
    displayQueueModeTask(&D_dryfield_night_saloon_g_r_80185068, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
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
        if (viewMask & D_dryfield_night_saloon_g_r_80185154[pointIndex]) {
            _glowDrawFlare(&gSaloonLightPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_STANDARD);
        }
    }
    for (pointIndex = 6; pointIndex < 11; pointIndex++) {
        if (viewMask & D_dryfield_night_saloon_g_r_80185154[pointIndex]) {
            _glowDrawFlare(&gSaloonLightPoints[pointIndex], 1, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_ALTERNATE);
        }
    }
    // Beam endpoints use the task transform rather than the flare world frame.
    if (viewMask & D_dryfield_night_saloon_g_r_80185154[DRYFIELD_NIGHT_SALOON_G_R_SHAFT_VIEW_MASK_INDEX]) {
        _glowDrawTwinShafts(coord);
    }
    if (viewMask & D_dryfield_night_saloon_g_r_80185154[DRYFIELD_NIGHT_SALOON_G_R_BEAM_VIEW_MASK_INDEX]) {
        _dryfieldNightSaloonGRDrawTaperedBeam(coord, &gSaloonLightPoints[DRYFIELD_NIGHT_SALOON_G_R_BEAM_START_POINT],
                                              &gSaloonLightPoints[DRYFIELD_NIGHT_SALOON_G_R_BEAM_END_POINT], DRYFIELD_NIGHT_SALOON_G_R_BEAM_RADIUS_SCALE);
    }
    // Six shaft vertices occupy points 14..19 but just one view-mask entry.
    for (pointIndex = 20; pointIndex < 23; pointIndex++) {
        if (viewMask & D_dryfield_night_saloon_g_r_80185154[pointIndex - DRYFIELD_NIGHT_SALOON_G_R_FINAL_FLARE_MASK_BIAS]) {
            _glowDrawFlare(&gSaloonLightPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_STANDARD);
        }
    }
    for (pointIndex = 23; pointIndex < (s32)ARRAY_SIZE(gSaloonLightPoints); pointIndex++) {
        if (viewMask & D_dryfield_night_saloon_g_r_80185154[pointIndex - DRYFIELD_NIGHT_SALOON_G_R_FINAL_FLARE_MASK_BIAS]) {
            _glowDrawFlare(&gSaloonLightPoints[pointIndex], 0, DRYFIELD_NIGHT_SALOON_G_R_FLARE_RADIUS_LARGE);
        }
    }
}

#include "../../shared/glow_draw_flare.inc.c"

#include "../../shared/glow_draw_twin_shafts.inc.c"

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
