#include "rooms/mist_r18.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/starter_inventory.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"
#include "../../shared/backdrop_crossfade.h"

/// Packet placement and storage layout of this room's captions and captured backdrop.
enum {
    MIST_R18_CAPTION_SPRITE_OT_SLOT = 4,
    MIST_R18_CAPTION_PLATE_OT_SLOT  = 5,
    MIST_R18_TEXTURE_DEPTH_4BIT     = 0,
    MIST_R18_TEXTURE_SHADE_UNITY    = 0x80,
    MIST_R18_BACKDROP_LEFT_VRAM_X   = 832,
    MIST_R18_BACKDROP_RIGHT_VRAM_X  = 640,
    MIST_R18_BACKDROP_RIGHT_VRAM_Y  = 256,
};

/// Next briefing script to start after the current event and menu have finished.
enum {
    MIST_R18_BRIEFING_PLACE_PROP         = 1,
    MIST_R18_BRIEFING_FIRST_HELD_MODEL   = 2,
    MIST_R18_BRIEFING_OPEN_KEY_ITEMS     = 3,
    MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP = 4,
    MIST_R18_BRIEFING_DEPARTING          = 5,
};

static WorldCoordRoomLights _gMistR18RoomLights;

static void _modelPlacementAttachPartTask(Task* childTask);
static void _mistR18AdvanceBriefingState(Task* unusedTask);
static void _mistR18DrawFadeSprites(s32 fadeActive, s32 shade);
static void _mistR18CaptureBackdropState(Task* task);
static void _mistR18AttachedModelIdleState(Task* task);
static void _mistR18DrawTile(const PrimDrawParams* draw);
static void _mistR18DrawSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY);
static void _mistR18SetTexturePage(s16 blendMode, s16 vramX, s16 vramY, s32 orderingTableSlot);
static void _mistR18WaitForCrossfadeViewState(Task* task);
static void _mistR18InitBriefingState(Task* task);

static TaskDesc   _gMistR18TaskDescs[8];
extern EvsCommand D_mist_r18_8018522C[];
extern EvsCommand D_mist_r18_8018576C[];
extern EvsCommand D_mist_r18_80185AE4[];
extern EvsCommand D_mist_r18_80185EBC[];
extern EvsCommand D_mist_r18_8018603C[];
extern EvsCommand D_mist_r18_801861BC[];
extern EvsCommand D_mist_r18_8018639C[];
extern EvsCommand D_mist_r18_8018645C[];
extern EvsCommand D_mist_r18_8018651C[];
extern EvsCommand D_mist_r18_80186564[];
/// Saved task drawing the first briefing model on the player's part 8.
///
/// NULL before spawning or after explicit teardown, including spawn failure.
/// Its attachment joins the player's teardown tree; clear this handle before
/// using it again if that tree has been killed. Room setup requires no live
/// previous instance because it resets the handle without killing a task.
static Task* _gMistR18FirstAttachedModelTask;

/// Saved task drawing the second briefing model on the player's part 8.
///
/// NULL before spawning or after explicit teardown, including spawn failure.
/// Its attachment joins the player's teardown tree; clear this handle before
/// using it again if that tree has been killed. Room setup requires no live
/// previous instance because it resets the handle without killing a task.
static Task* _gMistR18SecondAttachedModelTask;

/// Saved task drawing the briefing's separately placed prop.
///
/// Each spawn overwrites the handle, including with NULL on failure; scripts
/// tear down the previous instance before spawning again. Explicit teardown
/// kills a non-NULL task and clears the handle. Room setup only clears it,
/// so no previous instance may remain live when setup runs.
static Task* _gMistR18PlacedPropTask;

/// Next briefing phase to launch after the current event and menu finish.
///
/// Uses MIST_R18_BRIEFING_* values 1 place prop, 2 first held model, 3 open
/// key items, 4 check the Dryfield-map choice and 5 departure already started.
/// Phase 4 repeats the reminder until the map is selected; phase 5 launches
/// nothing further. This signed word is separate from the room task's state.
static s32 _gMistR18BriefingStep;
/// Set by `_mistR18AdvanceBriefingState` when the alternate cutscene branch ran.
extern s32 D_mist_r18_80186EA0;

/// Attachment, idle and teardown callbacks for the room's two held models.
///
/// Indexed by task state (0 attach to the parent part, 1 retain attachment,
/// 2 kill), without a terminator or bounds check. Dispatch copies all three
/// pointers by value; the callback code must remain loaded through the call.
static const TaskFuncTable3 _gMistR18AttachedModelStates = {
    { _modelPlacementAttachPartTask, _mistR18AttachedModelIdleState, taskKill },
};

/// State handlers of the room's cutscene task `mistR18BriefingTask`
/// dispatches: set-up, the cutscene step, then `taskKill`.
static const TaskFuncTable3 D_mist_r18_8017D5D0 = {
    { _mistR18InitBriefingState, _mistR18AdvanceBriefingState, taskKill },
};

/// Capture, view-wait, crossfade and teardown callbacks for the frozen backdrop.
///
/// Indexed by task state (0 capture, 1 wait for the replacement view,
/// 2 crossfade to the live frame, 3 kill). Dispatch copies all four pointers
/// by value and performs no bounds check; keep their code loaded through use.
static const TaskFuncTable4 _gMistR18CrossfadeStates = {
    { _mistR18CaptureBackdropState, _mistR18WaitForCrossfadeViewState, _crossfadeOutState, taskKill },
};

extern AreaResource D_mist_r18_80186BD8[3];

static WorldCollisionGrid _gMistR18FloorCollisionGrid;

/// Attached-model selectors passed by the briefing scripts and their parent part.
enum {
    MIST_R18_ATTACHMENT_MODEL_FIRST  = 0,
    MIST_R18_ATTACHMENT_MODEL_SECOND = 1,
    MIST_R18_ATTACHMENT_PLAYER_PART  = 8,
};
static void _mistR18SpawnAttachedModel(s32 modelIndex);
static void _mistR18KillAttachedModel(s32 modelIndex);
static void _mistR18SpawnBriefingCaption(void);
static void _mistR18StartBackdropCrossfade(void);
static void _mistR18SpawnPlacedProp(void);
static void _mistR18KillPlacedProp(void);
static void _mistR18ReloadParkingWithStarterLoadout(void);
static void _mistR18PrepareBriefingScene(void);
static void _mistR18StartSceneScriptPause(void);
static void _mistR18EnqueueScenePlayback(void);
static void _mistR18FinishSceneStream(void);
static void _mistR18CancelScene(void);
static void _mistR18SetOrderingDepthShift(s8 depthShift);
static void _mistR18OpenKeyItemMenu(void);

static AnimationSet _gMistR18Animation02274;
static AnimationSet _gMistR18Animation030E8;
static AnimationSet _gMistR18Animation0375C;
static AnimationSet _gMistR18Animation03C90;
static AnimationSet _gMistR18Animation0478C;
static AnimationSet _gMistR18Animation04DD4;
static AnimationSet _gMistR18Animation05AA4;
static AnimationSet _gMistR18Animation07230;
static AnimationSet _gMistR18Animation075CC;
static AnimationSet _gMistR18Animation078C0;
static TmdSource    _gMistR18Actor213000Model072AC;
static TmdSource    _gMistR18Actor213000Prop;
static void         _mistR18CaptionTask(Task* task);
static void         _mistR18TextureFadeTask(Task* task);
static void         _mistR18AttachedModelTask(Task* task);
static void         _mistR18FadeTileTask(Task* task);
static void         _mistR18CrossfadeTask(Task* task);
static void         _mistR18PlacePropTask(Task* task);
static void         _mistR18ReleaseScriptPauseTask(Task* task);

static TmdBone _gMistR18Actor213000Model072ACSkeleton[1] = {
#include "assets/actor_213000_model_072AC_skeleton.inc"
};

static u32 _gMistR18Actor213000Model072ACPartVerts[1] = {
#include "assets/actor_213000_model_072AC_partVerts.inc"
};

static SVECTOR _gMistR18Actor213000Model072ACVerts[14] = {
#include "assets/actor_213000_model_072AC_verts.inc"
};

static SVECTOR _gMistR18Actor213000Model072ACNormals[17] = {
#include "assets/actor_213000_model_072AC_normals.inc"
};

static u32 _gMistR18Actor213000Model072ACStream[98] = {
#include "assets/actor_213000_model_072AC_stream.inc"
};

static TmdSource _gMistR18Actor213000Model072AC = {
    0,
    652,
    0,
    1,
    _gMistR18Actor213000Model072ACPartVerts,
    _gMistR18Actor213000Model072ACVerts,
    _gMistR18Actor213000Model072ACNormals,
    _gMistR18Actor213000Model072ACSkeleton,
    _gMistR18Actor213000Model072ACStream,
};

static TmdBone _gMistR18Actor213000PropSkeleton[1] = {
#include "assets/actor_213000_prop_skeleton.inc"
};

static u32 _gMistR18Actor213000PropPartVerts[1] = {
#include "assets/actor_213000_prop_partVerts.inc"
};

static SVECTOR _gMistR18Actor213000PropVerts[14] = {
#include "assets/actor_213000_prop_verts.inc"
};

static u32 _gMistR18Actor213000PropStream[79] = {
#include "assets/actor_213000_prop_stream.inc"
};

static TmdSource _gMistR18Actor213000Prop = {
    0,
    528,
    0,
    1,
    _gMistR18Actor213000PropPartVerts,
    _gMistR18Actor213000PropVerts,
    &_gMistR18Actor213000PropVerts[14],
    _gMistR18Actor213000PropSkeleton,
    _gMistR18Actor213000PropStream,
};

static AnimationPackedPose _gMistR18Animation02274Bank1[7] = {
#include "assets/mist_r18_animation_02274_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation02274Bank4[113] = {
#include "assets/mist_r18_animation_02274_bank4.inc"
};

static AnimationRecord _gMistR18Animation02274Records[221] = {
#include "assets/mist_r18_animation_02274_records.inc"
};

static u16 _gMistR18Animation02274Indices[20] = {
#include "assets/mist_r18_animation_02274_indices.inc"
};

static AnimationSet _gMistR18Animation02274 = {
    _gMistR18Animation02274Records,
    _gMistR18Animation02274Indices,
    { NULL, _gMistR18Animation02274Bank1, NULL, NULL, _gMistR18Animation02274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation030E8Bank1[5] = {
#include "assets/mist_r18_animation_030E8_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation030E8Bank4[395] = {
#include "assets/mist_r18_animation_030E8_bank4.inc"
};

static AnimationRecord _gMistR18Animation030E8Records[495] = {
#include "assets/mist_r18_animation_030E8_records.inc"
};

static u16 _gMistR18Animation030E8Indices[20] = {
#include "assets/mist_r18_animation_030E8_indices.inc"
};

static AnimationSet _gMistR18Animation030E8 = {
    _gMistR18Animation030E8Records,
    _gMistR18Animation030E8Indices,
    { NULL, _gMistR18Animation030E8Bank1, NULL, NULL, _gMistR18Animation030E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation0375CBank1[7] = {
#include "assets/mist_r18_animation_0375C_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation0375CBank4[154] = {
#include "assets/mist_r18_animation_0375C_bank4.inc"
};

static AnimationRecord _gMistR18Animation0375CRecords[218] = {
#include "assets/mist_r18_animation_0375C_records.inc"
};

static u16 _gMistR18Animation0375CIndices[20] = {
#include "assets/mist_r18_animation_0375C_indices.inc"
};

static AnimationSet _gMistR18Animation0375C = {
    _gMistR18Animation0375CRecords,
    _gMistR18Animation0375CIndices,
    { NULL, _gMistR18Animation0375CBank1, NULL, NULL, _gMistR18Animation0375CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation03C90Bank1[2] = {
#include "assets/mist_r18_animation_03C90_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation03C90Bank4[95] = {
#include "assets/mist_r18_animation_03C90_bank4.inc"
};

static AnimationRecord _gMistR18Animation03C90Records[212] = {
#include "assets/mist_r18_animation_03C90_records.inc"
};

static u16 _gMistR18Animation03C90Indices[20] = {
#include "assets/mist_r18_animation_03C90_indices.inc"
};

static AnimationSet _gMistR18Animation03C90 = {
    _gMistR18Animation03C90Records,
    _gMistR18Animation03C90Indices,
    { NULL, _gMistR18Animation03C90Bank1, NULL, NULL, _gMistR18Animation03C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation0478CBank1[16] = {
#include "assets/mist_r18_animation_0478C_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation0478CBank4[285] = {
#include "assets/mist_r18_animation_0478C_bank4.inc"
};

static AnimationRecord _gMistR18Animation0478CRecords[350] = {
#include "assets/mist_r18_animation_0478C_records.inc"
};

static u16 _gMistR18Animation0478CIndices[20] = {
#include "assets/mist_r18_animation_0478C_indices.inc"
};

static AnimationSet _gMistR18Animation0478C = {
    _gMistR18Animation0478CRecords,
    _gMistR18Animation0478CIndices,
    { NULL, _gMistR18Animation0478CBank1, NULL, NULL, _gMistR18Animation0478CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation04DD4Bank1[18] = {
#include "assets/mist_r18_animation_04DD4_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation04DD4Bank4[59] = {
#include "assets/mist_r18_animation_04DD4_bank4.inc"
};

static AnimationRecord _gMistR18Animation04DD4Records[269] = {
#include "assets/mist_r18_animation_04DD4_records.inc"
};

static u16 _gMistR18Animation04DD4Indices[20] = {
#include "assets/mist_r18_animation_04DD4_indices.inc"
};

static AnimationSet _gMistR18Animation04DD4 = {
    _gMistR18Animation04DD4Records,
    _gMistR18Animation04DD4Indices,
    { NULL, _gMistR18Animation04DD4Bank1, NULL, NULL, _gMistR18Animation04DD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation05AA4Bank1[18] = {
#include "assets/mist_r18_animation_05AA4_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation05AA4Bank4[342] = {
#include "assets/mist_r18_animation_05AA4_bank4.inc"
};

static AnimationRecord _gMistR18Animation05AA4Records[404] = {
#include "assets/mist_r18_animation_05AA4_records.inc"
};

static u16 _gMistR18Animation05AA4Indices[20] = {
#include "assets/mist_r18_animation_05AA4_indices.inc"
};

static AnimationSet _gMistR18Animation05AA4 = {
    _gMistR18Animation05AA4Records,
    _gMistR18Animation05AA4Indices,
    { NULL, _gMistR18Animation05AA4Bank1, NULL, NULL, _gMistR18Animation05AA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation07230Bank1[2] = {
#include "assets/mist_r18_animation_07230_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation07230Bank4[685] = {
#include "assets/mist_r18_animation_07230_bank4.inc"
};

static AnimationRecord _gMistR18Animation07230Records[796] = {
#include "assets/mist_r18_animation_07230_records.inc"
};

static u16 _gMistR18Animation07230Indices[20] = {
#include "assets/mist_r18_animation_07230_indices.inc"
};

static AnimationSet _gMistR18Animation07230 = {
    _gMistR18Animation07230Records,
    _gMistR18Animation07230Indices,
    { NULL, _gMistR18Animation07230Bank1, NULL, NULL, _gMistR18Animation07230Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation075CCBank1[3] = {
#include "assets/mist_r18_animation_075CC_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation075CCBank4[86] = {
#include "assets/mist_r18_animation_075CC_bank4.inc"
};

static AnimationRecord _gMistR18Animation075CCRecords[116] = {
#include "assets/mist_r18_animation_075CC_records.inc"
};

static u16 _gMistR18Animation075CCIndices[20] = {
#include "assets/mist_r18_animation_075CC_indices.inc"
};

static AnimationSet _gMistR18Animation075CC = {
    _gMistR18Animation075CCRecords,
    _gMistR18Animation075CCIndices,
    { NULL, _gMistR18Animation075CCBank1, NULL, NULL, _gMistR18Animation075CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMistR18Animation078C0Bank1[6] = {
#include "assets/mist_r18_animation_078C0_bank1.inc"
};

static AnimationPackedRotation _gMistR18Animation078C0Bank4[52] = {
#include "assets/mist_r18_animation_078C0_bank4.inc"
};

static AnimationRecord _gMistR18Animation078C0Records[99] = {
#include "assets/mist_r18_animation_078C0_records.inc"
};

static u16 _gMistR18Animation078C0Indices[20] = {
#include "assets/mist_r18_animation_078C0_indices.inc"
};

static AnimationSet _gMistR18Animation078C0 = {
    _gMistR18Animation078C0Records,
    _gMistR18Animation078C0Indices,
    { NULL, _gMistR18Animation078C0Bank1, NULL, NULL, _gMistR18Animation078C0Bank4, NULL, NULL, NULL },
};

/// Glyph-byte script for the briefing's two-line date and M.I.S.T. location caption.
///
/// The first 56 bytes select resident cells 0..69 or a line break, followed by
/// TEXT_STREAM_END and three retained zero bytes. The reveal's post-end probe
/// is inside this array; readable storage for its predecessor probe is unproven.
static u8 _gMistR18BriefingCaptionScript[60] = {
    18,
    30,
    41,
    45,
    30,
    38,
    27,
    30,
    43,
    69,
    56,
    65,
    69,
    69,
    53,
    61,
    61,
    61,
    69,
    69,
    52,
    62,
    55,
    55,
    0,
    12,
    TEXT_STREAM_LINE_BREAK,
    12,
    66,
    8,
    66,
    18,
    66,
    19,
    66,
    69,
    2,
    30,
    39,
    45,
    30,
    43,
    65,
    69,
    69,
    11,
    40,
    44,
    69,
    0,
    39,
    32,
    30,
    37,
    30,
    44,
    TEXT_STREAM_END,
    0,
    0,
    0,
};

/// Mutable date-and-place caption state borrowed by the briefing's single reveal task.
///
/// Draws at (-150, -90) pixels with 13-pixel line spacing. Stored plate dimensions
/// are 216 by 29; the emitter subtracts one from each dimension.
/// Uses the room font at VRAM (704, 48) and palette (16, 260). The child resets
/// cursor, reveals a script byte every two ticks and reloads 45 for the final
/// hold. Keep this stream, script, font pixels and palette live until teardown;
/// simultaneous reveals would share the cursor and are not supported.
static TextStream _gMistR18BriefingCaptionStream = {
    .x           = -150,
    .y           = -90,
    .tpageX      = 704,
    .tpageY      = 48,
    .clutX       = 16,
    .clutY       = 260,
    .charDelay   = 1,
    .cursor      = 0,
    .chars       = _gMistR18BriefingCaptionScript,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = 13,
    .delayReload = 45,
    .boxWidth    = 216,
    .boxHeight   = 29,
};

/// Execution priority shared by the briefing's directly spawned child tasks.
enum { MIST_R18_BRIEFING_TASK_PRIORITY = 192 };

/// Spawn recipes for the briefing's held models, prop and presentation tasks.
///
/// Direct spawns use indices 0..7; there is no terminator. Slots 0/1 attach
/// models, 2 fades textured rectangles, 3 crossfades the backdrop, 4 places the
/// prop, 5 reveals the caption, 6 draws a fade tile and 7 monitors script pause.
/// Slots 2 and 6 have no room-local spawner or event-command reference.
/// Descriptors are read synchronously; callbacks and borrowed model sources
/// remain live with this overlay until their spawned tasks are torn down.
static TaskDesc _gMistR18TaskDescs[8] = {
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18AttachedModelTask, { .model = &_gMistR18Actor213000Model072AC } },
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18AttachedModelTask, { .model = &_gMistR18Actor213000Prop } },
    { { { TASK_BODY_COORD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18TextureFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18CrossfadeTask, { .value = 0 } },
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18PlacePropTask, { .model = &_gMistR18Actor213000Prop } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18CaptionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18FadeTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18ReleaseScriptPauseTask, { .value = 0 } },
};

/// Player-bank extension: NULL at slot 47, followed by briefing clips 48..57.
///
/// Copied by value into the equipped bank; the ten referenced room-owned sets
/// remain borrowed during playback. The complete table is eleven pointer words.
static AnimationSet* _gMistR18PlayerBriefingAnimationSets[11] = {
    NULL,
    &_gMistR18Animation02274,
    &_gMistR18Animation030E8,
    &_gMistR18Animation0375C,
    &_gMistR18Animation03C90,
    &_gMistR18Animation0478C,
    &_gMistR18Animation04DD4,
    &_gMistR18Animation05AA4,
    &_gMistR18Animation07230,
    &_gMistR18Animation075CC,
    &_gMistR18Animation078C0,
};

/// Installs all eleven briefing-extension words in the equipped player bank.
///
/// Synchronous dispatch copies slots 47..57 without starting playback. Requests
/// for clips 48..57 require this copy first and room clip data live during use.
static AnimationBankCopyRequest _gMistR18PlayerBriefingBankCopyRequest = {
    .source    = { .sets = _gMistR18PlayerBriefingAnimationSets },
    .wordCount = ARRAY_SIZE(_gMistR18PlayerBriefingAnimationSets),
};

/// Indexed clips and transition timing used by the briefing requests.
enum {
    MIST_R18_PLAYER_STORED_BANK_INDEX  = 1, // EVS replaces this value in its local request copy.
    MIST_R18_PLACED_ACTOR_BANK         = 0,
    MIST_R18_PLAYER_BASE_CLIP_1        = 1,
    MIST_R18_PLAYER_BRIEFING_CLIP_48   = ANIMATION_BANK_BASE_SET_COUNT + 1,
    MIST_R18_PLAYER_BRIEFING_CLIP_49   = ANIMATION_BANK_BASE_SET_COUNT + 2,
    MIST_R18_PLAYER_BRIEFING_CLIP_50   = ANIMATION_BANK_BASE_SET_COUNT + 3,
    MIST_R18_PLAYER_BRIEFING_CLIP_51   = ANIMATION_BANK_BASE_SET_COUNT + 4,
    MIST_R18_PLAYER_BRIEFING_CLIP_52   = ANIMATION_BANK_BASE_SET_COUNT + 5,
    MIST_R18_PLAYER_BRIEFING_CLIP_53   = ANIMATION_BANK_BASE_SET_COUNT + 6,
    MIST_R18_PLAYER_BRIEFING_CLIP_54   = ANIMATION_BANK_BASE_SET_COUNT + 7,
    MIST_R18_PLAYER_BRIEFING_CLIP_55   = ANIMATION_BANK_BASE_SET_COUNT + 8,
    MIST_R18_PLAYER_BRIEFING_CLIP_56   = ANIMATION_BANK_BASE_SET_COUNT + 9,
    MIST_R18_PLAYER_BRIEFING_CLIP_57   = ANIMATION_BANK_BASE_SET_COUNT + 10,
    MIST_R18_BALDWIN_CLIP_1            = 1,
    MIST_R18_BALDWIN_CLIP_2            = 2,
    MIST_R18_BALDWIN_CLIP_3            = 3,
    MIST_R18_BALDWIN_CLIP_4            = 4,
    MIST_R18_BALDWIN_CLIP_5            = 5,
    MIST_R18_BALDWIN_CLIP_6            = 6,
    MIST_R18_BALDWIN_CLIP_7            = 7,
    MIST_R18_RUPERT_CLIP_1             = 1,
    MIST_R18_PLACED_ACTOR_BLEND_FRAMES = 6,
};

/// Starts base player clip 1 for the introductory scene and prop-placement interlude.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
static AnimationPlayRequest _gMistR18PlayerBaseClip1Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BASE_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 48 after the introductory scene placement.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip48Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_48,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 49 at the next introductory dialogue cue.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip49Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 50 while the first held model is attached.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip50Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 51 during the first held-model exchange.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip51Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_51,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 52 at a later first held-model dialogue cue.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip52Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 53 before the first held model is released.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip53Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 54 while the second held model is attached.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip54Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 55 during the departure dialogue.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip55Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 56 before the second held model is released.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip56Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_56,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts the player's briefing clip 57 before opening the key-item menu.
///
/// EVS resolves the equipped-weapon bank in a local copy; the stored bank 1
/// is ignored. Resets playback without blending and requests disabled world
/// collision. Borrowed through script dispatch; the selected bank and clip
/// data must remain loaded during playback.
/// Requires the briefing bank-extension copy before dispatch.
static AnimationPlayRequest _gMistR18PlayerBriefingClip57Request = {
    .source               = { .index = MIST_R18_PLAYER_STORED_BANK_INDEX },
    .animationId          = MIST_R18_PLAYER_BRIEFING_CLIP_57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Baldwin's bank-0 clip 1 at briefing boundaries and before the key-item menu.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Resets the slots; stored blend duration 0 and collision setting are ignored.
static AnimationPlayRequest _gMistR18BaldwinResetClip1Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts Baldwin's bank-0 clip 1 after the introductory scene placement.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Resets the slots; stored blend duration 0 and collision setting are ignored.
static AnimationPlayRequest _gMistR18BaldwinIntroClip1Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 2 during the introductory dialogue.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Interpolates from initialized slots over six normal-rate frames.
/// The collision setting is ignored by this actor.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip2Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_2,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_R18_PLACED_ACTOR_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 3 during the introductory dialogue.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Interpolates from initialized slots over six normal-rate frames.
/// The collision setting is ignored by this actor.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip3Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_R18_PLACED_ACTOR_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 4 during the first held-model exchange.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Interpolates from initialized slots over six normal-rate frames.
/// The collision setting is ignored by this actor.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip4Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_R18_PLACED_ACTOR_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 5 during the first held-model exchange.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Interpolates from initialized slots over six normal-rate frames.
/// The collision setting is ignored by this actor.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip5Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_R18_PLACED_ACTOR_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 6 during the first held-model exchange.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Interpolates from initialized slots over six normal-rate frames.
/// The collision setting is ignored by this actor.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip6Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_R18_PLACED_ACTOR_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Baldwin's bank-0 clip 7 during the departure dialogue.
///
/// Targets placed scene actor 1. The receiver consumes this request
/// synchronously and borrows its own actor-package clip data during playback.
/// Resets the slots; stored blend duration 1 and collision setting are ignored.
static AnimationPlayRequest _gMistR18BaldwinBriefingClip7Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_BALDWIN_CLIP_7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_mist_r18_80185114 = { { .sets = NULL }, 8, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185128 = { { .sets = NULL }, 9, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018513C = { { .sets = NULL }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts hurt Rupert's bank-0 clip 1 at briefing boundaries and before the key-item menu.
///
/// Targets placed scene actor 0. Resets the slots; blend duration and world
/// collision setting are ignored. Dispatch borrows this request synchronously;
/// the actor-package bank and clip data remain borrowed during playback.
static AnimationPlayRequest _gMistR18RupertResetClip1Request = {
    .source               = { .index = MIST_R18_PLACED_ACTOR_BANK },
    .animationId          = MIST_R18_RUPERT_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_mist_r18_80185164 = { { .sets = NULL }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_80185178 = { { .sets = NULL }, 2, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_8018518C = { { .sets = NULL }, 3, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851A0 = { { .sets = NULL }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851B4 = { { .sets = NULL }, 5, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851C8 = { { .sets = NULL }, 6, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851DC = { { .sets = NULL }, 7, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_r18_801851F0 = { { .sets = NULL }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player placement restored between briefing scenes: (0, 128, 0), facing zero.
///
/// Position uses whole world-coordinate units; Euler angles use 4096 units per
/// turn. Placement messages borrow both vectors until dispatch and copy them
/// into the player; this overlay must remain loaded through those messages.
static ActorTransform _gMistR18BriefingPlayerPlacement = { { 0, 128, 0, 0 }, { 0, 0, 0, 0 } };

ActorCommand D_mist_r18_8018521C = { 0 };

/// Baldwin's actor-command IDs used when the briefing hides his held models.
enum {
    MIST_R18_BALDWIN_COMMAND_HIDE_FIRST_HELD_MODEL  = 1,
    MIST_R18_BALDWIN_COMMAND_HIDE_SECOND_HELD_MODEL = 3,
};

/// Hides Baldwin's first held model at scene completion or when the introduction is skipped.
///
/// Sent to placed scene actor 1. Only active drawing is disabled; the child task
/// remains owned by Baldwin. The receiver ignores the zero context tag and
/// borrows this four-byte command only through synchronous dispatch.
static ActorCommand _gMistR18BaldwinHideFirstHeldModelCommand = {
    .context = { .key = 0 },
    .command = MIST_R18_BALDWIN_COMMAND_HIDE_FIRST_HELD_MODEL,
};

ActorCommand D_mist_r18_80185224 = { { .loc = { 0, 0 } }, 2 };

/// Hides Baldwin's second held model at the first held-model exchange's end or skip.
///
/// Sent to placed scene actor 1. Only active drawing is disabled; the child task
/// remains owned by Baldwin. The receiver ignores the zero context tag and
/// borrows this four-byte command only through synchronous dispatch.
static ActorCommand _gMistR18BaldwinHideSecondHeldModelCommand = {
    .context = { .key = 0 },
    .command = MIST_R18_BALDWIN_COMMAND_HIDE_SECOND_HELD_MODEL,
};

EvsCommand D_mist_r18_8018522C[56] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18PrepareBriefingScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18StartSceneScriptPause }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18SpawnPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBaseClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18SpawnBriefingCaption }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18StartBackdropCrossfade }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinIntroClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185164 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18FinishSceneStream }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51120003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51120004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_8018521C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistR18BaldwinHideFirstHeldModelCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018576C[37] = {
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistR18SpawnAttachedModel }, { .value = MIST_R18_ATTACHMENT_MODEL_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185178 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_8018518C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_r18_80185224 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18KillPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistR18KillAttachedModel }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistR18BaldwinHideSecondHeldModelCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80185AE4[41] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistR18SpawnAttachedModel }, { .value = MIST_R18_ATTACHMENT_MODEL_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _mistR18SetOrderingDepthShift }, { .value = DISPLAY_DEPTH_SHIFT_4X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _mistR18SetOrderingDepthShift }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinBriefingClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185114 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_80185128 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistR18KillAttachedModel }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _mistR18SetOrderingDepthShift }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18ReloadParkingWithStarterLoadout }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80185EBC[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBaseClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18KillPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18SpawnPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018603C[16] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18BaldwinResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistR18RupertResetClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18OpenKeyItemMenu }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_801861BC[20] = {
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistR18BriefingPlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMistR18PlayerBriefingBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistR18PlayerBriefingClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_8018513C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_r18_801851F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18OpenKeyItemMenu }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018639C[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistR18BaldwinHideFirstHeldModelCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018645C[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18KillPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistR18KillAttachedModel }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistR18BaldwinHideSecondHeldModelCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_8018651C[3] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _mistR18SetOrderingDepthShift }, { .value = DISPLAY_DEPTH_SHIFT_1X }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18ReloadParkingWithStarterLoadout }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_r18_80186564[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18KillPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistR18SpawnPlacedProp }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

WorldCollisionRoomResources gMistR18CollisionRoomResources[1] = {
    { &_gMistR18FloorCollisionGrid, NULL, NULL, NULL },
};

u8* gMistR18ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gMistR18ViewCounts[2] = { ARRAY_SIZE(gMistR18ViewCameras), 0 };

WorldCoordRoomLighting gMistR18RoomLightingTable[1] = {
    { &_gMistR18RoomLights, NULL },
};

DirectionWarpEntry gMistR18WarpEntries[1] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -6000, 0, 3218 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -6000, 0, 3218 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

static SVECTOR _gMistR18FloorCollisionGridNormals[1] = {
#include "assets/mist_r18_collision_09138_normals.inc"
};

static SVECTOR _gMistR18FloorCollisionGridVerts[4] = {
#include "assets/mist_r18_collision_09138_verts.inc"
};

static WorldCollisionGridFace _gMistR18FloorCollisionGridFaces[1] = {
#include "assets/mist_r18_collision_09138_faces.inc"
};

static s16 _gMistR18FloorCollisionGridCells[24] = {
#include "assets/mist_r18_collision_09138_cells.inc"
};

#define GRID_CELL(i) (&_gMistR18FloorCollisionGridCells[i])
static s16* _gMistR18FloorCollisionGridTable[12] = {
#include "assets/mist_r18_collision_09138_table.inc"
};
#undef GRID_CELL

/// The briefing room's class-1 floor and its X-major collision-cell index.
///
/// One quad at Y=0 spans X=-7000..7000 and Z=-5000..6000 in game units.
/// The 4-by-3 table has 4000-unit cells; every cell selects face 0 and ends
/// with WORLD_COLLISION_GRID_CELL_END. Activation binds the writable descriptor
/// to the current view; the descriptor and its five asset arrays stay live
/// with this room overlay while collision queries use them.
static WorldCollisionGrid _gMistR18FloorCollisionGrid = {
    NULL,
    _gMistR18FloorCollisionGridNormals,
    _gMistR18FloorCollisionGridVerts,
    _gMistR18FloorCollisionGridFaces,
    _gMistR18FloorCollisionGridTable,
    7000,
    5000,
    4,
    3,
    4000,
    ARRAY_SIZE(_gMistR18FloorCollisionGridFaces),
};

ViewCamera gMistR18ViewCameras[10] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x6978, -500 } }, 447 },
    { { { { 1821, 0, 3668 }, { 23, 4095, -11 }, { -3668, 25, 1821 } }, { -1440, 1500, -620 } }, 230 },
    { { { { 1698, 0, 3727 }, { 836, 3991, -381 }, { -3632, 919, 1655 } }, { 1290, 1500, -3770 } }, 257 },
    { { { { -1337, 0, 3871 }, { 8, 4095, 3 }, { -3871, 9, -1337 } }, { 1970, 1440, -4180 } }, 329 },
    { { { { -3828, 0, -1455 }, { 265, 4027, -699 }, { 1430, -748, -3764 } }, { 5550, 800, -5320 } }, 680 },
    { { { { 1376, 0, 3857 }, { 1511, 3768, -539 }, { -3549, 1605, 1266 } }, { 4520, 1670, -2880 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
    { { { { 2471, 0, -3266 }, { 87, 4094, 66 }, { 3264, -110, 2470 } }, { 5490, 1190, -3000 } }, 257 },
};

/// Background-only ranges for mapped view 1; the zero first count keeps image strips.
static SpriteBatch _gMistR18View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 2, including two further empty ranges.
///
/// The zero first count keeps image strips. The second record's serialized
/// byte pair {1, 0} has no established role; all counts and the end marker stay intact.
static SpriteBatch _gMistR18View2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 3; the zero first count keeps image strips.
static SpriteBatch _gMistR18View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit sprite texture page at (960, 0), with its CLUT at (0, 255).
enum {
    MIST_R18_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    MIST_R18_SPRITE_TEXTURE_PAGE       = getTPage(MIST_R18_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    MIST_R18_SPRITE_CLUT               = getClut(0, 255),
};

/// Ten depth-sorted foreground rectangles over mapped view 4's decoded background.
///
/// XY and dimensions are pixels relative to the drawing origin, UV is in page
/// texels, and depths are unscaled OT sorting values. The batch list selects
/// every element; source geometry remains writable while the room is loaded.
static SpriteSource _gMistR18View4SpriteSources[10] = {
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 64, 40 } }, -56, 80, 750, { .fields = { 64, 0 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 64, 40 } }, 8, 80, 750, { .fields = { 64, 40 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 40 } }, 72, 80, 750, { .fields = { 88, 80 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 48, 24 } }, -56, 56, 825, { .fields = { 80, 120 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 24 } }, -8, 56, 825, { .fields = { 112, 144 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 24 } }, 64, 56, 825, { .fields = { 112, 168 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 56, 24 } }, 8, 56, 825, { .fields = { 72, 192 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 8, 24 } }, 80, 56, 825, { .fields = { 120, 216 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 8, 24 } }, 88, 56, 825, { .fields = { 112, 216 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 16 } }, 96, 64, 825, { .fields = { 112, 240 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
};

/// Mapped view 4's decoded background followed by its complete foreground range.
static SpriteBatch _gMistR18View4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistR18View4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 5; the zero first count keeps image strips.
static SpriteBatch _gMistR18View5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seventeen depth-sorted foreground rectangles over mapped view 6's decoded background.
///
/// XY and dimensions are drawing-origin pixels, UV is in texture-page texels,
/// and depths are unscaled OT sorting values. The batch list selects every
/// element; source geometry remains writable while the room is loaded.
static SpriteSource _gMistR18View6SpriteSources[17] = {
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 32, 32 } }, -96, 88, 288, { .fields = { 56, 168 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, 72, 323, { .fields = { 32, 56 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 40 } }, -64, 80, 298, { .fields = { 88, 216 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 16 } }, -64, 64, 352, { .fields = { 32, 136 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 48 } }, -24, 72, 310, { .fields = { 88, 168 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 16 } }, -24, 56, 350, { .fields = { 32, 120 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 56 } }, 16, 64, 318, { .fields = { 88, 0 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 48, 56 } }, 56, 64, 283, { .fields = { 80, 56 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 56, 56 } }, 104, 64, 318, { .fields = { 72, 112 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 32, 16 } }, 16, 48, 350, { .fields = { 32, 40 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 40, 362, { .fields = { 56, 96 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 24, 24 } }, 64, 40, 362, { .fields = { 64, 232 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 32, 32 } }, 88, 32, 362, { .fields = { 56, 200 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 40, 375, { .fields = { 64, 72 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 48, 375, { .fields = { 48, 152 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 16, 32 } }, -152, 88, 307, { .fields = { 64, 40 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
    { MIST_R18_SPRITE_TEXTURE_PAGE, MIST_R18_SPRITE_CLUT, { .fields = { 40, 40 } }, -136, 80, 302, { .fields = { 48, 0 } }, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, MIST_R18_TEXTURE_SHADE_UNITY, 0 },
};

/// Mapped view 6's decoded background followed by its complete foreground range.
static SpriteBatch _gMistR18View6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistR18View6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 7; the zero first count keeps image strips.
static SpriteBatch _gMistR18View7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 8; the zero first count keeps image strips.
static SpriteBatch _gMistR18View8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 9; the zero first count keeps image strips.
static SpriteBatch _gMistR18View9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only ranges for mapped view 10; the zero first count keeps image strips.
static SpriteBatch _gMistR18View10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMistR18SpriteViews[10] = {
    { { .empty = _gMistR18View1SpriteBatches }, _gMistR18View1SpriteBatches, NULL },
    { { .empty = _gMistR18View2SpriteBatches }, _gMistR18View2SpriteBatches, NULL },
    { { .empty = _gMistR18View3SpriteBatches }, _gMistR18View3SpriteBatches, NULL },
    { { .elements = _gMistR18View4SpriteSources }, _gMistR18View4SpriteBatches, NULL },
    { { .empty = _gMistR18View5SpriteBatches }, _gMistR18View5SpriteBatches, NULL },
    { { .elements = _gMistR18View6SpriteSources }, _gMistR18View6SpriteBatches, NULL },
    { { .empty = _gMistR18View7SpriteBatches }, _gMistR18View7SpriteBatches, NULL },
    { { .empty = _gMistR18View8SpriteBatches }, _gMistR18View8SpriteBatches, NULL },
    { { .empty = _gMistR18View9SpriteBatches }, _gMistR18View9SpriteBatches, NULL },
    { { .empty = _gMistR18View10SpriteBatches }, _gMistR18View10SpriteBatches, NULL },
};

AreaResource D_mist_r18_80186BD8[3] = {
    { 144, 130, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor113000RupertBroderickHurtTaskDesc },
    { 105, 130, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor213000EricBaldwinTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_mist_r18_80186BFC[13] = {
    { NULL, NULL },
    { gMapAkropolisMistR18Placements, D_mist_r18_80186BD8 },
    { NULL, NULL },
    { gMapAkropolisMistR18Placements, D_mist_r18_80186BD8 },
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

/// Five authored point lights contributing in every briefing view.
///
/// Positions and inner/outer falloff distances use whole world-coordinate units;
/// RGB 2867 uses the 4096-unit intensity scale. The first four lights fade over
/// distances 2000..3000 and the fifth over 800..1200. Setup and shading mutate
/// transform caches and attenuation, so these records stay writable and loaded.
static WorldCoordPointLight _gMistR18PointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5700, -2300, 2050 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5700, -2300, 4800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3800, -2300, 4800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3800, -2300, 2050 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5450, -1200, 4000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 800, 1200 },
};

/// Room light collection selecting all five point lights and no directional or cone lights.
///
/// Borrows the mutable point-light array for the lifetime of this room overlay.
static WorldCoordRoomLights _gMistR18RoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gMistR18PointLights),
    _gMistR18PointLights,
    0,
    NULL,
};

/// Type-1 script request bases for the room's paired walking and running cues.
enum {
    MIST_R18_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 45,
    MIST_R18_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 47,
};

/// Configured footstep cue bases for every surface; scripted jumps reuse walking cues.
///
/// Cue 2 requests the base, cue 1 adds one, and companion playback adds 100.
/// The surface policy borrows this room-owned record while the overlay is loaded.
static WorldCollisionFootstepSounds _gMistR18FootstepSounds = {
    MIST_R18_FOOTSTEP_WALK,
    MIST_R18_FOOTSTEP_RUN,
    MIST_R18_FOOTSTEP_WALK,
};

/// Common surface policy: block probes, allow weapon impacts and apply pushback.
///
/// All classes borrow the same footstep requests. The leading byte's role is unproven.
static WorldCollisionSurfaceProperties _gMistR18SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gMistR18FootstepSounds,
};

WorldCollisionSurfaceProperties* gMistR18SurfacePropertiesByClass[8] = {
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
    &_gMistR18SurfaceProperties,
};

s32 D_mist_r18_80186EA0;

/// Reveals a glyph-script caption over a translucent black plate during an event.
///
/// Borrows the mutable `TextStream` in `spawnArg2.pointer`, its script and glyph
/// table until teardown. `charDelay` and `delayReload` are callback ticks;
/// a negative character delay reveals the complete caption on initialization.
/// Uses unmasked glyph bytes (the room script has no high-bit glyph indices),
/// centered draw-environment pixels and a plate inset of three pixels. The
/// compiled probe reads the byte before `chars` at cursor zero and one byte
/// after the terminator; the script storage must make both reads valid.
/// State 0 initializes, state 1 redraws and advances the cursor, and all other
/// states kill the task. Leaving the event also requests teardown.
static void _mistR18CaptionTask(Task* task)
{
    enum {
        MIST_R18_CAPTION_INITIALIZE = 0,
        MIST_R18_CAPTION_REVEAL     = 1,
        MIST_R18_CAPTION_FINISHED   = -1,
        MIST_R18_FONT_PAGE_U_MASK   = 0x3F,
    };
    PrimDrawParams draw;
    TextStream*    stream;
    s32            byteIndex;

    /// Draws the revealed prefix, leaving the pen after its final glyph.
    ///
    /// Uses this function's `stream`, `draw` and `byteIndex` locals, updating
    /// the pen and loop index. Borrows script/glyph data and emits sprite
    /// packets; glyph bytes must index the cell table directly. No early exits.
#define MIST_R18_DRAW_CAPTION_PREFIX()                                                                              \
    {                                                                                                               \
        for (byteIndex = 0; byteIndex < stream->cursor; byteIndex++) {                                              \
            if (stream->chars[byteIndex] == TEXT_STREAM_LINE_BREAK) {                                               \
                draw.x  = stream->x;                                                                                \
                draw.y += stream->lineHeight;                                                                       \
            } else {                                                                                                \
                draw.u = stream->glyphs[stream->chars[byteIndex]].u + (stream->tpageX & MIST_R18_FONT_PAGE_U_MASK); \
                draw.v = stream->glyphs[stream->chars[byteIndex]].v + (u8)stream->tpageY;                           \
                draw.w = stream->glyphs[stream->chars[byteIndex]].width;                                            \
                draw.h = stream->glyphs[stream->chars[byteIndex]].height;                                           \
                if (draw.h != 0) {                                                                                  \
                    _mistR18DrawSprite(&draw, stream->clutX, stream->clutY);                                        \
                }                                                                                                   \
                draw.x += stream->glyphs[stream->chars[byteIndex]].width;                                           \
            }                                                                                                       \
        }                                                                                                           \
    }

    stream = task->spawnArg2.pointer;
    if (gGameSession->eventState == 0) {
        task->state = MIST_R18_CAPTION_FINISHED;
    }

    switch (task->state) {
        case MIST_R18_CAPTION_INITIALIZE:
            // Reset reveal timing; a negative delay exposes the complete script.
            stream->cursor = 0;
            if (stream->charDelay < 0) {
                byteIndex = 0;
                if (stream->chars[0] != TEXT_STREAM_END) {
                    do {
                        byteIndex++;
                        stream->cursor++;
                    } while (stream->chars[byteIndex] != TEXT_STREAM_END);
                }
                task->killCountdown = stream->delayReload;
            } else {
                task->killCountdown = stream->charDelay;
            }
            task->state++;
            return;

        case MIST_R18_CAPTION_REVEAL:
            // Redraw the revealed prefix, then advance its byte cursor on expiry.
            draw.x         = stream->x;
            draw.y         = stream->y;
            draw.u         = stream->tpageX;
            draw.v         = stream->tpageY;
            draw.r         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.g         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.b         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.semiTrans = 0;
            draw.unused_12 = ONE;

            if (stream->chars[stream->cursor - 1] != TEXT_STREAM_END) {
                MIST_R18_DRAW_CAPTION_PREFIX();

                _mistR18SetTexturePage(GPU_BLEND_ADD, stream->tpageX, stream->tpageY, MIST_R18_CAPTION_SPRITE_OT_SLOT);

                if (--task->killCountdown < 0) {
                    stream->cursor++;
                    if (stream->chars[stream->cursor] == TEXT_STREAM_END) {
                        task->killCountdown = stream->delayReload;
                    } else {
                        task->killCountdown = stream->charDelay;
                    }
                }

                // The plate sits three pixels above and left of the pen.
                draw.x         = stream->x - 3;
                draw.y         = stream->y - 3;
                draw.w         = stream->boxWidth;
                draw.h         = stream->boxHeight;
                draw.b         = 0;
                draw.g         = 0;
                draw.r         = 0;
                draw.semiTrans = 1;
                _mistR18DrawTile(&draw);
                _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, MIST_R18_CAPTION_PLATE_OT_SLOT);
                return;
            }
            task->state++;
            return;

        default:
            break;
    }
    taskKill(task);
#undef MIST_R18_DRAW_CAPTION_PREFIX
}

/// Starts a skippable briefing event and commits the cursor for its successor.
///
/// Both script arrays remain live through event playback. The interpreter marks
/// the event active before the cursor advances, preventing another launch.
/// Call only while EVS is idle. `nextStep` is the briefing cursor to run when
/// playback ends; it is independent of the room task's state.
static inline void _mistR18StartSkippableBriefingStep(EvsCommand* script, EvsCommand* skipScript, s32 nextStep)
{
    evsStartScriptWithSkip(script, EVENT_SCRIPT_HUD_HIDE_RESTORE, skipScript);
    _gMistR18BriefingStep = nextStep;
}

/// Advances the briefing scripts and repeats the key-item menu until the Dryfield map is opened.
///
/// Runs only while the event interpreter is idle, the attachment wheel is closed
/// and no display-mode transition is pending. The script cursor is independent
/// of the task's state. A repeated menu preserves its map-opened latch; departure
/// starts once the map choice is latched. `unusedTask` is required by task dispatch.
static void _mistR18AdvanceBriefingState(Task* unusedTask)
{
    enum { MIST_R18_EVENT_IDLE = 0 };
    s32 briefingStep;

    // Event completion and menu teardown must finish before another script starts.
    if ((gGameSession->eventState == MIST_R18_EVENT_IDLE) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        briefingStep = _gMistR18BriefingStep;
        if (briefingStep == MIST_R18_BRIEFING_PLACE_PROP) {
            _mistR18StartSkippableBriefingStep(D_mist_r18_80185EBC, D_mist_r18_80186564, MIST_R18_BRIEFING_FIRST_HELD_MODEL);
        } else if (briefingStep == MIST_R18_BRIEFING_FIRST_HELD_MODEL) {
            evsStartScriptWithSkip(D_mist_r18_8018576C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mist_r18_8018645C);
            D_mist_r18_80186EA0   = false;
            _gMistR18BriefingStep = MIST_R18_BRIEFING_OPEN_KEY_ITEMS;
        } else if (briefingStep == MIST_R18_BRIEFING_OPEN_KEY_ITEMS) {
            evsStartScript(D_mist_r18_8018603C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            _gMistR18BriefingStep = MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP;
        } else if (briefingStep == MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP) {
            // Reopen the reminder menu without resetting its map-opened latch.
            if (mapAkropolisWasDryfieldMapSelected() != true) {
                evsStartScript(D_mist_r18_801861BC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                D_mist_r18_80186EA0 = true;
                return;
            }
            _mistR18StartSkippableBriefingStep(D_mist_r18_80185AE4, D_mist_r18_8018651C, MIST_R18_BRIEFING_DEPARTING);
        }
    }
}

/// Fades two fixed textured rectangles in, holds them opaque, then fades them out.
///
/// `killCountdown` holds RGB modulation, stepped by 21 per callback; the ramp
/// reaches 147 before switching to opaque raw-texture drawing. `spawnArg1.value`
/// counts hold callbacks, shortened by event skipping. State 0 queues the zero
/// level twice, states 1/3 ramp, state 2 holds, and later states kill the task.
/// This is task-table entry 2; no room-local spawner or event command uses it.
static void _mistR18TextureFadeTask(Task* task)
{
    enum {
        MIST_R18_TEXTURE_FADE_INITIALIZE = 0,
        MIST_R18_TEXTURE_FADE_IN         = 1,
        MIST_R18_TEXTURE_FADE_HOLD       = 2,
        MIST_R18_TEXTURE_FADE_OUT        = 3,
        MIST_R18_TEXTURE_FADE_STEP       = 21,
    };
    s32 fadeActive;

    fadeActive = 1;
    switch (task->state) {
        case MIST_R18_TEXTURE_FADE_INITIALIZE:
            task->killCountdown = 0;
            _mistR18DrawFadeSprites(1, 0);
            task->state++;
            break;
        case MIST_R18_TEXTURE_FADE_IN:
            task->killCountdown += MIST_R18_TEXTURE_FADE_STEP;
            if (task->killCountdown >= MIST_R18_TEXTURE_SHADE_UNITY + 1) {
                fadeActive = 0;
                task->state++;
            }
            break;
        case MIST_R18_TEXTURE_FADE_HOLD:
            fadeActive = 0;
            task->spawnArg1.value--;
            if ((task->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
                task->state++;
            }
            break;
        case MIST_R18_TEXTURE_FADE_OUT:
            task->killCountdown -= MIST_R18_TEXTURE_FADE_STEP;
            if (task->killCountdown < MIST_R18_TEXTURE_FADE_STEP + 1) {
                task->state++;
            }
            break;
        default:
            taskKill(task);
            return;
    }
    _mistR18DrawFadeSprites(fadeActive, task->killCountdown);
}

/// Queues the two fixed 4-bit texture rectangles used by the room's texture fade.
///
/// `fadeActive` selects modulated semitransparent drawing when nonzero, or
/// opaque raw-texture drawing when zero. `shade` supplies RGB modulation through
/// its low byte. Samples page (704, 0), with palettes at (0, 271)/(16, 271).
/// Reserves two `SPRT`s and one `DR_TPAGE` in the frame arena; the additive page
/// command executes first because OT insertion prepends packets. Requires that
/// texture/palette data and packet storage remain live until the GPU finishes.
static void _mistR18DrawFadeSprites(s32 fadeActive, s32 shade)
{
    enum {
        MIST_R18_SPRITE_RAW_OPAQUE_CODE      = 0x65,
        MIST_R18_SPRITE_MODULATED_BLEND_CODE = 0x66,
        MIST_R18_FADE_FIRST_CLUT             = getClut(0, 271),
        MIST_R18_FADE_SECOND_CLUT            = getClut(16, 271),
        MIST_R18_FADE_TEXTURE_PAGE           = getTPage(MIST_R18_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 704, 0),
    };
    SPRT*     sprite;
    DR_TPAGE* pageCommand;
    s16       screenX;
    s16       screenY;

    screenX        = -0x96;
    screenY        = -0x5A;
    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    if (fadeActive == 0) {
        sprite->code = MIST_R18_SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = MIST_R18_SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    sprite->clut = MIST_R18_FADE_FIRST_CLUT;
    setWH(sprite, 0xCF, 0x23);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);

    screenX        = -0x22;
    screenY        = 0x36;
    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    if (fadeActive == 0) {
        sprite->code = MIST_R18_SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = MIST_R18_SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0x24);
    sprite->clut = MIST_R18_FADE_SECOND_CLUT;
    setWH(sprite, 0xB7, 0x23);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    setDrawTPage(pageCommand, true, false, MIST_R18_FADE_TEXTURE_PAGE);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, pageCommand);
}

/// Captures the current 320x240 draw framebuffer as the crossfade's saved backdrop.
///
/// Splits at column 192, copying to VRAM (832, 0) and (640, 256). Buffer 0
/// starts at (0, 0), buffer 1 at (0, 272). The frame must already be rendered
/// when OT slot 8 executes. Reserves two `DR_MOVE`s and two `DR_STP`s, borrowed
/// by the GPU until frame completion. Resets the shade and advances the state.
static void _mistR18CaptureBackdropState(Task* task)
{
    enum {
        MIST_R18_FRAMEBUFFER_LOWER_Y = 272,
    };
    RECT     sourceRect;
    DR_STP*  maskCommand;
    DR_MOVE* copyCommand;
    s16      frameX;
    s16      frameY;

    if (gDisplayState.drawBuffer == 0) {
        frameX = 0;
        frameY = 0;
    } else {
        frameX = 0;
        frameY = MIST_R18_FRAMEBUFFER_LOWER_Y;
    }

    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, false);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskCommand);

    copyCommand    = gGpuPrimCursor;
    gGpuPrimCursor = copyCommand + 1;
    sourceRect.x   = frameX;
    sourceRect.y   = frameY;
    sourceRect.w   = CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.h   = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, MIST_R18_BACKDROP_LEFT_VRAM_X, 0);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    copyCommand    = gGpuPrimCursor;
    gGpuPrimCursor = copyCommand + 1;
    sourceRect.x   = frameX + CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.y   = frameY;
    sourceRect.w   = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.h   = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, MIST_R18_BACKDROP_RIGHT_VRAM_X, MIST_R18_BACKDROP_RIGHT_VRAM_Y);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    // Head insertion executes mask-on, right copy, left copy, then mask-off.
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, true);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskCommand);

    task->killCountdown = 0;
    task->state++;
}

#include "../../shared/backdrop_crossfade_live.inc.c"

/// Queues an additive redraw of the captured backdrop for the room crossfade.
///
/// Samples 192x240 pixels at VRAM (832, 0) and 128x240 at (640, 256), placing
/// them over the centered 320x240 frame. `shade` is RGB modulation (0 black,
/// 0x80 unchanged); only its low byte is stored. Requires the completed capture
/// and room for two `SPRT`s and two `DR_TPAGE`s in the current frame arena.
/// The GPU borrows those packets until completion; no capacity checks are made.
static void _crossfadeDrawBackdrop(s32 shade)
{
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
    _crossfadeSetTpage(MIST_R18_BACKDROP_LEFT_VRAM_X, 0);

    // Each prepended texture-page command executes before its sprite.
    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = CROSSFADE_BACKDROP_LEFT_WIDTH - CROSSFADE_BACKDROP_WIDTH / 2;
    sprite->y0   = -CROSSFADE_BACKDROP_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = CROSSFADE_BACKDROP_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(MIST_R18_BACKDROP_RIGHT_VRAM_X, MIST_R18_BACKDROP_RIGHT_VRAM_Y);
#undef CROSSFADE_ALLOCATE_BACKDROP_SPRITE
}

/// Dispatches attachment, idle and teardown for either of the room's held models.
///
/// Requires a live TMD task and a state in 0..2. Setup borrows the parent TMD
/// task in `spawnArg2.pointer`; `spawnArg1.value` indexes its part coordinates.
/// The parent coordinate and lighting matrices must outlive the child. Setup
/// joins the parent's teardown tree; state 1 keeps the attachment unchanged,
/// and state 2 kills it. The three callbacks are copied by value before dispatch.
static void _mistR18AttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistR18AttachedModelStates;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Keeps an attached model unchanged while its parent controls its lifetime.
static void _mistR18AttachedModelIdleState(Task* task)
{
}

/// Draws a translucent black rectangle until its callback countdown expires.
///
/// In state 0, borrows the `RECT` in `spawnArg2.pointer` on every callback and
/// covers (w - 1) by (h - 1) draw-environment pixels using average blending.
/// Decrements `spawnArg1.value` after drawing; a nonpositive result or any other
/// state kills the task. The rectangle must remain live until teardown.
/// Task-table entry 6 has no room-local spawner or event-command reference.
static void _mistR18FadeTileTask(Task* task)
{
    enum { MIST_R18_FADE_TILE_DRAW = 0 };
    PrimDrawParams draw;
    const RECT*    rect;

    rect = task->spawnArg2.pointer;

    if (task->state == MIST_R18_FADE_TILE_DRAW) {
        draw.x         = rect->x;
        draw.y         = rect->y;
        draw.w         = rect->w;
        draw.h         = rect->h;
        draw.b         = 0;
        draw.g         = 0;
        draw.r         = 0;
        draw.semiTrans = 1;
        _mistR18DrawTile(&draw);
        _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, MIST_R18_CAPTION_PLATE_OT_SLOT);

        if (--task->spawnArg1.value > 0) {
            return;
        }
    }
    taskKill(task);
}

/// Queues a coloured tile in the caption plate OT slot.
///
/// Borrows `draw` only for this call; it supplies screen X/Y, RGB and dimensions
/// whose low halfwords become (w - 1)/(h - 1). Nonzero `semiTrans` enables GPU
/// blending; the caller supplies its draw-mode command. Reserves one `TILE` in
/// the frame arena, retained until the GPU completes; capacity is not checked.
static void _mistR18DrawTile(const PrimDrawParams* draw)
{
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    if (draw->semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
    tile->r0 = draw->r;
    tile->g0 = draw->g;
    tile->b0 = draw->b;
    tile->x0 = draw->x;
    tile->y0 = draw->y;
    tile->w  = draw->w - 1;
    tile->h  = draw->h - 1;
    AddPrim(gGpuCurrentOt + MIST_R18_CAPTION_PLATE_OT_SLOT, tile);
}

/// Queues a caption sprite with the palette at VRAM `clutX`/`clutY`.
///
/// Borrows `draw` for the call. X/Y are draw-environment pixels, U/V take their
/// low byte, and the dimensions become (w - 1)/(h - 1). Palette X is a pixel
/// coordinate rounded down to a 16-pixel boundary; Y is a VRAM row.
/// Nonzero `semiTrans` selects modulated blending,
/// otherwise the texture is opaque and unmodulated. The caller queues the
/// texture page separately. Reserves one `SPRT` until the GPU finishes.
static void _mistR18DrawSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY)
{
    SPRT* sprite;
    u8    textureV;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    SetSprt(sprite);
    if (draw->semiTrans == 0) {
        SetShadeTex(sprite, 1);
        SetSemiTrans(sprite, 0);
    } else {
        SetShadeTex(sprite, 0);
        SetSemiTrans(sprite, 1);
    }
    sprite->r0   = draw->r;
    sprite->g0   = draw->g;
    sprite->b0   = draw->b;
    sprite->x0   = draw->x;
    sprite->y0   = draw->y;
    sprite->u0   = draw->u;
    textureV     = draw->v;
    sprite->clut = getClut(clutX, clutY);
    sprite->v0   = textureV;
    sprite->w    = draw->w - 1;
    sprite->h    = draw->h - 1;
    AddPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);
}

/// Queues a 4-bit texture-page command before sprites in the selected OT slot.
///
/// `vramX`/`vramY` are signed 16-bit VRAM pixel coordinates; the SDK packs their
/// page origin. The low two bits of `blendMode` select a `GPU_BLEND_*` mode.
/// `orderingTableSlot` indexes the current OT and must be in bounds. Enables
/// drawing in the display area, disables dithering, and reserves one `DR_TPAGE`
/// until the GPU completes. Queue sprites first: OT insertion prepends packets.
static void _mistR18SetTexturePage(s16 blendMode, s16 vramX, s16 vramY, s32 orderingTableSlot)
{
    DR_TPAGE* pageCommand;

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    SetDrawTPage(pageCommand, true, false, GetTPage(MIST_R18_TEXTURE_DEPTH_4BIT, blendMode, vramX, vramY));
    AddPrim(gGpuCurrentOt + orderingTableSlot, pageCommand);
}

/// Spawns either briefing model for attachment to the live player's part 8.
///
/// `modelIndex` is 0 first model or 1 second model; other values do nothing.
/// Spawns only when the selected saved handle is NULL, leaving it NULL on
/// failure. A successful model is drawable and its setup task later borrows
/// the player's part coordinate and lighting, joining the player's teardown
/// tree. The player/model resources must remain live through that attachment.
static void _mistR18SpawnAttachedModel(s32 modelIndex)
{
    Task** modelTaskSlot;
    Task*  modelTask;

    switch (modelIndex) {
        case MIST_R18_ATTACHMENT_MODEL_FIRST:
            modelTaskSlot = &_gMistR18FirstAttachedModelTask;
            break;
        case MIST_R18_ATTACHMENT_MODEL_SECOND:
            modelTaskSlot = &_gMistR18SecondAttachedModelTask;
            break;
        default:
            modelTaskSlot = NULL;
            break;
    }

    if ((modelTaskSlot != NULL) && (*modelTaskSlot == NULL)) {
        modelTask      = taskSpawnFromTable(_gMistR18TaskDescs, modelIndex, MIST_R18_ATTACHMENT_PLAYER_PART, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        *modelTaskSlot = modelTask;
        if (modelTask != NULL) {
            modelTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Kills and clears the selected held-model task (0 first model, 1 second model).
///
/// Other indices do nothing. The handle is cleared even when already NULL.
static void _mistR18KillAttachedModel(s32 modelIndex)
{
    enum {
        MIST_R18_ATTACHED_FIRST_MODEL  = 0,
        MIST_R18_ATTACHED_SECOND_MODEL = 1,
    };
    if (modelIndex == MIST_R18_ATTACHED_FIRST_MODEL) {
        if (_gMistR18FirstAttachedModelTask != NULL) {
            taskKill(_gMistR18FirstAttachedModelTask);
        }
        _gMistR18FirstAttachedModelTask = NULL;
    } else if (modelIndex == MIST_R18_ATTACHED_SECOND_MODEL) {
        if (_gMistR18SecondAttachedModelTask != NULL) {
            taskKill(_gMistR18SecondAttachedModelTask);
        }
        _gMistR18SecondAttachedModelTask = NULL;
    }
}

/// Starts the briefing's glyph-script caption over its translucent plate.
///
/// The child borrows and resets the room's mutable text stream. Keep that
/// stream, glyph cells and this overlay live until it finishes or the event
/// ends. The script starts one instance; spawn failure is ignored here.
static void _mistR18SpawnBriefingCaption(void)
{
    enum { MIST_R18_BRIEFING_CAPTION_TASK_INDEX = 5 };

    taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_BRIEFING_CAPTION_TASK_INDEX, 0, &_gMistR18BriefingCaptionStream);
}

/// Starts the briefing's captured-backdrop transition to the next view.
///
/// Captures the current backdrop, waits for the replacement view and fades to
/// its live frame. The script leaves view 2 after this request. Keep this
/// overlay and frame resources live through the child's teardown; its spawn
/// failure is ignored and no handle is retained here.
static void _mistR18StartBackdropCrossfade(void)
{
    enum { MIST_R18_BACKDROP_CROSSFADE_TASK_INDEX = 3 };

    taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_BACKDROP_CROSSFADE_TASK_INDEX, 0, NULL);
}

/// Dispatches the room's captured-backdrop crossfade.
///
/// Requires a live task with state 0 capture, 1 wait for the replacement view,
/// 2 fade to the live frame or 3 teardown. Handlers keep RGB modulation in
/// `killCountdown` and borrow frame packets until GPU completion. The callback
/// table is copied by value before dispatch; there is no index check.
static void _mistR18CrossfadeTask(Task* task)
{
    TaskFuncTable4 states;

    states = _gMistR18CrossfadeStates;
    states.funcs[task->state](task);
}

/// Waits for the crossfade's replacement view while advancing its saved shade.
///
/// Adds eight per callback with 16-bit wrapping, clamping the signed result at
/// 64. Once the view is ready or the view index leaves 2, sets shade 128 and
/// advances to the crossfade state. This state queues no drawing packets.
static void _mistR18WaitForCrossfadeViewState(Task* task)
{
    enum {
        MIST_R18_CROSSFADE_WAIT_STEP  = 8,
        MIST_R18_CROSSFADE_WAIT_SHADE = 64,
        MIST_R18_CROSSFADE_WAIT_VIEW  = 2,
    };
    u16 shade;

    shade               = (u16)task->killCountdown + MIST_R18_CROSSFADE_WAIT_STEP;
    task->killCountdown = shade;
    if ((s16)shade >= MIST_R18_CROSSFADE_WAIT_SHADE) {
        task->killCountdown = MIST_R18_CROSSFADE_WAIT_SHADE;
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->location.loc.view != MIST_R18_CROSSFADE_WAIT_VIEW)) {
        task->killCountdown = CROSSFADE_SHADE_UNITY;
        task->state++;
    }
}

#include "../../shared/backdrop_crossfade_out.inc.c"

#include "../../shared/backdrop_crossfade_tpage.inc.c"

/// Spawns the briefing's separately placed prop and saves its task handle.
///
/// Requires the room's prop model and task table to remain loaded. Each call
/// overwrites the saved handle, including on allocation failure; event scripts
/// pair successful spawns with teardown before spawning again.
static void _mistR18SpawnPlacedProp(void)
{
    enum { MIST_R18_PLACED_PROP_TASK_INDEX = 4 };

    _gMistR18PlacedPropTask = taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_PLACED_PROP_TASK_INDEX, 0, 0);
}

/// Kills the room's separately placed prop task and clears its saved handle.
static void _mistR18KillPlacedProp(void)
{
    if (_gMistR18PlacedPropTask != NULL) {
        taskKill(_gMistR18PlacedPropTask);
    }
    _gMistR18PlacedPropTask = NULL;
}

/// Places the room's prop model once at its fixed cutscene transform.
///
/// Requires a live TMD body with root coordinate 0. State 0 sets its translation
/// in parent-coordinate units and Euler angles in 4096 units per turn, rebuilds
/// the rotation, invalidates composition, applies an OT bias of -8 and enables
/// drawing. Later states retain the placement; task teardown owns the model.
static void _mistR18PlacePropTask(Task* task)
{
    enum {
        MIST_R18_PROP_PLACE_INITIALIZE = 0,
        MIST_R18_PROP_ORDERING_BIAS    = -8,
    };
    GfxCoord*  root;
    TmdObject* model;

    if (task->state == MIST_R18_PROP_PLACE_INITIALIZE) {
        root               = task->extra.tmd->coords;
        root->coord.t[0]   = -0x1496;
        root->coord.t[1]   = -0x2DA;
        root->coord.t[2]   = 0xB90;
        root->param.rot.vx = 0x6AA;
        root->param.rot.vy = -0xF8E;
        root->param.rot.vz = -0x333;
        RotMatrixZYX(&root->param.rot, &root->coord);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        model              = task->extra.tmd;
        model->otOffset    = MIST_R18_PROP_ORDERING_BIAS;
        model->flags      &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state++;
    }
}

/// Reloads the M.I.S.T. parking area with the starter inventory after the briefing.
///
/// Resets the carried loadout and queues the resident reload task for Acropolis,
/// parking room 3 and arrival 3 with sprite variant 1. Stops non-ambient sound
/// scripts without fading. Used by both briefing completion paths; the saved
/// destination is consumed by the reload task after this callback returns.
static void _mistR18ReloadParkingWithStarterLoadout(void)
{
    enum {
        MIST_R18_PARKING_ENTRY_WARP     = 3,
        MIST_R18_PARKING_ENTRY_ROOM     = 3,
        MIST_R18_PARKING_SPRITE_VARIANT = 1,
    };
    inventoryInitializeStarterLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_MIST_PARKING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = MIST_R18_PARKING_ENTRY_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = MIST_R18_PARKING_ENTRY_ROOM;
    gDisplayState.spriteVariant                                 = MIST_R18_PARKING_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Selects the briefing's scene stream and stages its deferred audio-start request.
///
/// Marks the view dirty first. Requires the current stage's descriptor table to
/// contain scene key (1, 30, 11, 0) and no unfinished earlier scene selection.
/// Selection saves random state; the stream and buffers remain live through
/// playback. The resident CD dispatcher commits the deferred start separately.
static void _mistR18PrepareBriefingScene(void)
{
    enum {
        MIST_R18_BRIEFING_SCENE_GROUP     = 1,
        MIST_R18_BRIEFING_SCENE_STREAM_ID = 30,
        MIST_R18_BRIEFING_SCENE_SUB_ID    = 11,
    };
    gGameSession->viewDirty = true;
    cdCmdSelectScene(MIST_R18_BRIEFING_SCENE_GROUP, MIST_R18_BRIEFING_SCENE_STREAM_ID, MIST_R18_BRIEFING_SCENE_SUB_ID);
    cdCmdStageSceneAudioStart();
}

/// Pauses the briefing script after starting its scene-view monitor.
///
/// The pause latch is set only if the bodyless monitor task is allocated.
/// The monitor releases the latch when the scene view is no longer ready;
/// it stays alive until task-list teardown. Requires loaded room descriptors
/// and a live event interpreter; allocation failure leaves the latch unchanged.
static void _mistR18StartSceneScriptPause(void)
{
    enum { MIST_R18_SCENE_SCRIPT_PAUSE_TASK_INDEX = 7 };

    if (taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_SCENE_SCRIPT_PAUSE_TASK_INDEX, 0, NULL) != NULL) {
        D_801156F9 = 1;
    }
}

/// Queues playback of the briefing's previously selected scene/audio session.
///
/// Requires prepared playback buffers and space in the resident CD request ring.
/// An audio-free selection enters playing mode immediately.
static void _mistR18EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Ends briefing scene streaming and restores the random state saved at selection.
///
/// Requires a successful earlier scene selection. Buffer and task teardown remain
/// with their owners; pending resident CD requests are left in place.
static void _mistR18FinishSceneStream(void)
{
    streamFinishScene();
}

/// Requests scene cancellation when the briefing's introductory event is skipped.
///
/// Discards the deferred CD replacement and immediately finishes scene streaming,
/// restoring saved random state. Resident CD dispatch completes cancellation;
/// the skip script handles actor and display cleanup separately.
static void _mistR18CancelScene(void)
{
    cdCmdCancelScene();
}

/// Releases the event-script pause when the scene's view is no longer ready.
///
/// The spawner first sets the shared pause latch. Each callback clears it when
/// `viewReady != 1`; the task neither advances its state nor kills itself.
static void _mistR18ReleaseScriptPauseTask(Task* task)
{
    if (gGameSession->viewReady != 1) {
        D_801156F9 = 0;
    }
}

/// Sets the camera-depth left shift used before ordering-table quantization.
///
/// The departure script passes `DISPLAY_DEPTH_SHIFT_4X` while view 5 is active and
/// `DISPLAY_DEPTH_SHIFT_1X` to restore normal depth. The signed-byte EVS argument
/// is stored as a byte without validation; supported renderer shifts are 0..3.
static void _mistR18SetOrderingDepthShift(s8 depthShift)
{
    gDisplayState.otDepthShift = depthShift;
}

/// Opens the briefing's key-item menu without reinitializing it on reminder passes.
///
/// The first pass marks the four briefing items collected and clears the map
/// choice. Reminder passes skip that initialization. The map overlay and its UI
/// must stay loaded until the queued display-mode task finishes.
static void _mistR18OpenKeyItemMenu(void)
{
    mapAkropolisOpenKeyItemMenu(0, D_mist_r18_80186EA0);
}

/// Registers the room task and starts the introductory briefing event with its skip script.
///
/// Requires fresh prop handles: initialization forgets them without killing tasks.
/// Advances to task state 1 and seeds the separate cursor for the prop-placement
/// script, which starts after the introductory event finishes or is skipped.
static void _mistR18InitBriefingState(Task* task)
{
    _gMistR18FirstAttachedModelTask  = NULL;
    _gMistR18SecondAttachedModelTask = NULL;
    _gMistR18PlacedPropTask          = NULL;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    evsStartScriptWithSkip(D_mist_r18_8018522C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mist_r18_8018639C);
    task->state++;
    _gMistR18BriefingStep = MIST_R18_BRIEFING_PLACE_PROP;
}

void mistR18BriefingTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_mist_r18_8017D5D0;
    states.funcs[task->state](task);
}
