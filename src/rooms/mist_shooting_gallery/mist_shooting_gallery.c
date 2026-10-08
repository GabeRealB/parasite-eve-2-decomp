#include "rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "actors/actor_215100.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8018055c.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/glow_draw.h"
#include "../../shared/jukebox.h"

// Relocated CAP file slots selected by commands 5..8 and 0x21..0x22.
enum {
    MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS       = 1,
    MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS      = 3,
    MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_LOW_COMMANDS  = 0x300,
    MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_HIGH_COMMANDS = 0x2C0
};

// Key-item use requests carry an item ID and an unused second argument word.
enum { MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

// Colours of the mode-selection panels, packed with red in the low byte.
enum {
    MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR         = 0x606060,
    MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR = 0x0D287F
};

/// One string for each run mode, in the order of the save's `gameMode`
/// (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
///
/// The gallery keeps two: the mode names its mode list draws as rows, and the
/// descriptions its help panel shows for the mode in force. Both are constants
/// that their reader copies whole onto its stack before indexing, and the
/// struct is what lets a file-scope table be copied by assignment.
typedef struct {
    const char* byMode[4]; // Text for run mode 0..3
} _MistShootingGalleryModeTexts;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryModeTexts, 0x10);

/// A rating a row of the gallery's DATA panel can show: how far its gauge is
/// filled and the word printed beside it.
///
/// The panel rates the run mode in force on four rows - mission level,
/// condition, enemy level and supply level - and each row has a table of four
/// of these, one for every run mode in the order of the save's `gameMode`
/// (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
typedef struct {
    s32         level; // Marks drawn on the row's gauge (1 lowest .. 5 full)
    const char* name;  // Word for the rating ("EASY", "GOOD", "VERY POOR", ...)
} _MistShootingGalleryRating;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryRating, 0x8);

/// Score and label for one gallery target kind, as the RESULT panel prints it.
///
/// The table of these is indexed by target kind, the same index as
/// `MistShootingGalleryWork::kills`. `points` is added once for each kill of
/// that kind, and is negative when the kill is penalised.
typedef struct {
    s32         points; // Points added for each kill of this kind; negative deducts
    const char* name;   // Name printed for the kind ("Bacterium", "Woman", "Red Target", ...)
} _MistShootingGalleryTargetScore;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryTargetScore, 0x8);

extern UiObjectDesc D_mist_shooting_gallery_80185060;

extern TaskDesc D_mist_shooting_gallery_80184F8C;
extern TaskDesc D_mist_shooting_gallery_801850D0;

/// Text in the room's trailing data that the tables below point at.
extern char D_mist_shooting_gallery_80184DD4[];
extern char D_mist_shooting_gallery_80184E24[];
extern char D_mist_shooting_gallery_80184E70[];
extern char D_mist_shooting_gallery_80184EC4[];
extern char D_mist_shooting_gallery_80184F18[];
extern char D_mist_shooting_gallery_80184F1C[];
extern char D_mist_shooting_gallery_80184F20[];
extern char D_mist_shooting_gallery_80184F24[];
extern char D_mist_shooting_gallery_80184F2C[];

static const char D_mist_shooting_gallery_8017D65C[]; // "TOTAL SCORE"

static const _MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D6D8;
static const _MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D708;

static const char D_mist_shooting_gallery_8017D820[];
static const char D_mist_shooting_gallery_8017D828[];
static const char D_mist_shooting_gallery_8017D838[];
static const char D_mist_shooting_gallery_8017D844[];
static const char D_mist_shooting_gallery_8017D850[];

extern void func_actor_215100_8014A398(void);
extern s32  func_actor_215100_8014AA54(RoomEventMsg* loc);
extern void func_actor_215100_8014AB6C(void);
extern void func_actor_215100_8014C5E0(s16, s16, s16);

extern s32        D_actor_215100_8014D038;
extern TaskDesc   D_actor_215100_8014E13C[];
extern EvsCommand D_actor_215100_80153274[];
extern EvsCommand D_actor_215100_80153D6C[];

/// The ten weapons the gallery's weapon picker offers, in row order. Rows whose
/// weapon family is not owned (`inventoryIsItemLimitReached` returns 0) are skipped, so
/// `UiList::currentItemIndex` indexes the drawn rows rather than table slots.
extern s16 D_mist_shooting_gallery_80184F34[];

extern UiList D_mist_shooting_gallery_80184F4C;

extern UiList                          D_mist_shooting_gallery_8018503C;
extern UiObjectDesc                    D_mist_shooting_gallery_8018507C[];
extern UiObjectDesc                    D_mist_shooting_gallery_8018501C;
extern _MistShootingGalleryTargetScore D_mist_shooting_gallery_80184F98[MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT];
extern TaskMessageEntry                D_mist_shooting_gallery_801850E8[];

extern TaskDesc           D_mist_shooting_gallery_801850DC;
extern WorldCollisionGrid D_mist_shooting_gallery_80185198;
extern WorldCollisionGrid D_mist_shooting_gallery_801851F8;

/// The jukebox's track lists, one per game mode, each a run of track id and
/// name pairs.
extern JukeboxTrack gJukeboxTracksAttach0[];
extern JukeboxTrack gJukeboxTracksAttach1[];
extern JukeboxTrack gJukeboxTracksAttach2[];
extern JukeboxTrack gJukeboxTracksAttach3[];
extern JukeboxTrack gJukeboxTracksAttach4[];
extern JukeboxTrack gJukeboxTracks0[];
extern JukeboxTrack gJukeboxTracks1[];
extern JukeboxTrack gJukeboxTracks2[];
extern JukeboxTrack gJukeboxTracks3[];
extern JukeboxTrack gJukeboxTracks4[];

/// The jukebox menu's title, "SELECT". A stray 0xE1 byte follows its
/// terminator, so the block stays in assembly.
static const char D_mist_shooting_gallery_8017DB04[];

/// The jukebox's track list, whose row callback is
/// `_jukeboxDrawRow`.
extern UiList D_mist_shooting_gallery_80185338;

/// The jukebox panel's descriptor; its update routine is the menu task
/// `_mistShootingGalleryJukeboxPanelTask`.
extern UiObjectDesc gJukeboxPanelDesc;

extern TaskDesc D_mist_shooting_gallery_80185378;

static void _mistShootingGallerySetStoryBarrierLowered(s32 lowered);

static const char D_mist_shooting_gallery_8017D65C[];

static void _mistShootingGalleryJukeboxPanelTask(Task* task);
static void _mistShootingGalleryModeSplashTask(Task* task);
static void _mistShootingGalleryClearMovieTask(Task* task);
void        func_mist_shooting_gallery_801810D8(Task*);

extern const char D_mist_shooting_gallery_8017D86C[22];
extern const char D_mist_shooting_gallery_8017D884[19];
extern const char D_mist_shooting_gallery_8017D898[14];
extern const char D_mist_shooting_gallery_8017D8A8[26];
extern const char D_mist_shooting_gallery_8017D8C4[24];
extern const char D_mist_shooting_gallery_8017D8DC[17];
extern const char D_mist_shooting_gallery_8017D8F0[23];
extern const char D_mist_shooting_gallery_8017D908[17];
extern const char D_mist_shooting_gallery_8017D91C[16];
extern const char D_mist_shooting_gallery_8017D92C[15];
extern const char D_mist_shooting_gallery_8017D93C[24];
extern const char D_mist_shooting_gallery_8017D954[11];
extern const char D_mist_shooting_gallery_8017D960[27];
extern const char D_mist_shooting_gallery_8017D97C[11];
extern const char D_mist_shooting_gallery_8017D988[17];
extern const char D_mist_shooting_gallery_8017D99C[14];
extern const char D_mist_shooting_gallery_8017D9AC[11];
extern const char D_mist_shooting_gallery_8017D9B8[13];
extern const char D_mist_shooting_gallery_8017D9C8[23];
extern const char D_mist_shooting_gallery_8017D9E0[13];
extern const char D_mist_shooting_gallery_8017D9F0[17];
extern const char D_mist_shooting_gallery_8017DA04[16];
extern const char D_mist_shooting_gallery_8017DA14[12];
extern const char D_mist_shooting_gallery_8017DA20[20];
extern const char D_mist_shooting_gallery_8017DA34[17];
extern const char D_mist_shooting_gallery_8017DA48[19];
extern const char D_mist_shooting_gallery_8017DA5C[24];
extern const char D_mist_shooting_gallery_8017DA74[19];
extern const char D_mist_shooting_gallery_8017DA88[27];
extern const char D_mist_shooting_gallery_8017DAA4[18];
extern const char D_mist_shooting_gallery_8017DAB8[16];
extern const char D_mist_shooting_gallery_8017DAC8[20];
static s32        _mistShootingGalleryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32               func_mist_shooting_gallery_8017FEB8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32               func_mist_shooting_gallery_80180000(Task*, s32, s32, s32);
s32               func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void              func_mist_shooting_gallery_8017E234(Task*);
void              func_mist_shooting_gallery_8017E854(Task*);
void              func_mist_shooting_gallery_8017EAE0(Task*);
static void       _mistShootingGalleryModeStatusPanelTask(Task* task);
static void       _mistShootingGalleryModeDataPanelTask(Task* task);
void              func_mist_shooting_gallery_8017F6C8(Task*);
static void       _mistShootingGalleryModeRow(UiList* list, UiObject* object);
static void       _mistShootingGalleryModeHelpPanelTask(Task* task);
void              func_mist_shooting_gallery_8017FDD0(Task* task);

static const char D_mist_shooting_gallery_8017D5E0[12];
static const char D_mist_shooting_gallery_8017D5EC[16];
static const char D_mist_shooting_gallery_8017D5FC[16];
static const char D_mist_shooting_gallery_8017D60C[8];
static const char D_mist_shooting_gallery_8017D614[8];
static const char D_mist_shooting_gallery_8017D61C[4];
static const char D_mist_shooting_gallery_8017D620[8];
static const char D_mist_shooting_gallery_8017D628[8];
static const char D_mist_shooting_gallery_8017D630[12];
static const char D_mist_shooting_gallery_8017D63C[4];
static const char D_mist_shooting_gallery_8017D640[8];
static const char D_mist_shooting_gallery_8017D648[8];
static const char D_mist_shooting_gallery_8017D650[12];
void              func_mist_shooting_gallery_8017DE7C(UiList*, UiObject*);
static void       _mistShootingGalleryWeaponSelectPanelTask(Task* task);

char D_mist_shooting_gallery_80184DD4[80] = {
    82,
    101,
    112,
    108,
    97,
    121,
    32,
    109,
    111,
    100,
    101,
    10,
    67,
    111,
    108,
    108,
    101,
    99,
    116,
    32,
    98,
    111,
    110,
    117,
    115,
    32,
    105,
    116,
    101,
    109,
    115,
    32,
    101,
    97,
    99,
    104,
    32,
    116,
    105,
    109,
    101,
    32,
    121,
    111,
    117,
    10,
    99,
    108,
    101,
    97,
    114,
    32,
    116,
    104,
    101,
    32,
    103,
    97,
    109,
    101,
    32,
    105,
    110,
    32,
    114,
    101,
    112,
    108,
    97,
    121,
    32,
    109,
    111,
    100,
    101,
    33,
    0,
    0,
    0,
    0,
};

char D_mist_shooting_gallery_80184E24[76] = {
    66,
    111,
    117,
    110,
    116,
    121,
    32,
    109,
    111,
    100,
    101,
    10,
    70,
    105,
    110,
    100,
    32,
    116,
    104,
    101,
    32,
    104,
    105,
    100,
    100,
    101,
    110,
    32,
    71,
    79,
    76,
    69,
    77,
    32,
    115,
    111,
    108,
    100,
    105,
    101,
    114,
    115,
    10,
    97,
    110,
    100,
    32,
    115,
    116,
    114,
    105,
    118,
    101,
    32,
    102,
    111,
    114,
    32,
    97,
    32,
    66,
    80,
    32,
    104,
    105,
    103,
    104,
    32,
    115,
    99,
    111,
    114,
    101,
    33,
    0,
    0,
};

char D_mist_shooting_gallery_80184E70[84] = {
    83,
    99,
    97,
    118,
    101,
    110,
    103,
    101,
    114,
    32,
    109,
    111,
    100,
    101,
    10,
    83,
    104,
    111,
    112,
    115,
    32,
    97,
    114,
    101,
    32,
    98,
    97,
    114,
    101,
    44,
    32,
    97,
    110,
    100,
    32,
    112,
    105,
    99,
    107,
    105,
    110,
    103,
    115,
    32,
    97,
    114,
    101,
    10,
    115,
    108,
    105,
    109,
    46,
    32,
    85,
    115,
    101,
    32,
    121,
    111,
    117,
    114,
    32,
    119,
    105,
    116,
    115,
    44,
    32,
    110,
    111,
    116,
    32,
    121,
    111,
    117,
    114,
    32,
    97,
    109,
    109,
    111,
    33,
    0,
};

char D_mist_shooting_gallery_80184EC4[84] = {
    78,
    105,
    103,
    104,
    116,
    109,
    97,
    114,
    101,
    32,
    109,
    111,
    100,
    101,
    10,
    89,
    111,
    117,
    32,
    115,
    116,
    97,
    114,
    116,
    32,
    111,
    102,
    102,
    32,
    115,
    105,
    99,
    107,
    32,
    97,
    110,
    100,
    32,
    116,
    104,
    105,
    110,
    103,
    115,
    32,
    103,
    101,
    116,
    10,
    119,
    111,
    114,
    115,
    101,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    109,
    111,
    115,
    116,
    32,
    100,
    105,
    102,
    102,
    105,
    99,
    117,
    108,
    116,
    32,
    109,
    111,
    100,
    101,
    46,
    0,
};

char D_mist_shooting_gallery_80184F18[4] = {
    123,
    0,
    0,
    0,
};

char D_mist_shooting_gallery_80184F1C[4] = {
    123,
    123,
    0,
    0,
};

char D_mist_shooting_gallery_80184F20[4] = {
    123,
    123,
    123,
    0,
};

char D_mist_shooting_gallery_80184F24[8] = {
    123,
    123,
    123,
    123,
    0,
    0,
    0,
    0,
};

char D_mist_shooting_gallery_80184F2C[8] = {
    123,
    123,
    123,
    123,
    123,
    0,
    0,
    0,
};

s16 D_mist_shooting_gallery_80184F34[10] = {
    129,
    130,
    157,
    143,
    136,
    132,
    144,
    140,
    141,
    142,
};

UiListRowCallback D_mist_shooting_gallery_80184F48[1] = {
    func_mist_shooting_gallery_8017DE7C,
};

UiList D_mist_shooting_gallery_80184F4C = { D_mist_shooting_gallery_80184F48, 1, { .unsignedValue = 1 }, 0, 15, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc D_mist_shooting_gallery_80184F70 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 64 }, 32, 0, TASK_BODY_NONE, 192, _mistShootingGalleryWeaponSelectPanelTask, 0 };

TaskDesc D_mist_shooting_gallery_80184F8C = { { { TASK_BODY_NONE, 192 } }, Gp_MenuRootTask, { .value = 0 } };

_MistShootingGalleryTargetScore D_mist_shooting_gallery_80184F98[MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT] = {
    { 600, D_mist_shooting_gallery_8017D650 },
    { 1600, D_mist_shooting_gallery_8017D648 },
    { 900, D_mist_shooting_gallery_8017D640 },
    { 500, D_mist_shooting_gallery_8017D63C },
    { 2400, D_mist_shooting_gallery_8017D630 },
    { 500, D_mist_shooting_gallery_8017D628 },
    { 1300, D_mist_shooting_gallery_8017D620 },
    { 3000, D_mist_shooting_gallery_8017D61C },
    { 600, D_mist_shooting_gallery_8017D614 },
    { -3000, D_mist_shooting_gallery_8017D60C },
    { 200, D_mist_shooting_gallery_8017D5FC },
    { 400, D_mist_shooting_gallery_8017D5EC },
    { 600, D_mist_shooting_gallery_8017D5E0 },
};

UiObjectDesc D_mist_shooting_gallery_80185000 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -104, -48, 208, 64 }, 32, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017E234, 0 };

UiObjectDesc D_mist_shooting_gallery_8018501C = { USER_INTERFACE_PANEL_TITLE_STYLE, { -72, -48, 144, 56 }, 24, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017E854, 0 };

UiListRowCallback D_mist_shooting_gallery_80185038[1] = {
    _mistShootingGalleryModeRow,
};

UiList D_mist_shooting_gallery_8018503C = { D_mist_shooting_gallery_80185038, 4, { .unsignedValue = 4 }, 1, 15, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc D_mist_shooting_gallery_80185060 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -96, 176, 64 }, 32, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017EAE0, 0 };

UiObjectDesc D_mist_shooting_gallery_8018507C[3] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 32, -96, 112, 128 }, 28, 0, TASK_BODY_NONE, 192, _mistShootingGalleryModeStatusPanelTask, 0 },
    { 3, { -144, 32, 288, 48 }, 24, 0, TASK_BODY_NONE, 192, _mistShootingGalleryModeHelpPanelTask, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -22, 288, 73 }, 20, 0, TASK_BODY_NONE, 192, _mistShootingGalleryModeDataPanelTask, 0 },
};

TaskDesc D_mist_shooting_gallery_801850D0 = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_8017F6C8, { .value = 0 } };

TaskDesc D_mist_shooting_gallery_801850DC = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_8017FDD0, { .value = 0 } };

TaskMessageEntry D_mist_shooting_gallery_801850E8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mist_shooting_gallery_8017FEB8 },
    { MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM, _mistShootingGalleryRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mist_shooting_gallery_8018008C },
    { ROOM_MESSAGE_COMMAND, func_mist_shooting_gallery_80180000 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static SVECTOR _gMistShootingGalleryCollision07BD8Normals[3] = {
#include "assets/mist_shooting_gallery_collision_07BD8_normals.inc"
};

static SVECTOR _gMistShootingGalleryCollision07BD8Verts[8] = {
#include "assets/mist_shooting_gallery_collision_07BD8_verts.inc"
};

static WorldCollisionGridFace _gMistShootingGalleryCollision07BD8Faces[3] = {
#include "assets/mist_shooting_gallery_collision_07BD8_faces.inc"
};

static s16 _gMistShootingGalleryCollision07BD8Cells[4] = {
#include "assets/mist_shooting_gallery_collision_07BD8_cells.inc"
};

#define GRID_CELL(i) (&_gMistShootingGalleryCollision07BD8Cells[i])
static s16* _gMistShootingGalleryCollision07BD8Table[1] = {
#include "assets/mist_shooting_gallery_collision_07BD8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_shooting_gallery_80185198 = { NULL, _gMistShootingGalleryCollision07BD8Normals, _gMistShootingGalleryCollision07BD8Verts, _gMistShootingGalleryCollision07BD8Faces, _gMistShootingGalleryCollision07BD8Table, 0x28B9, -4925, 1, 1, 4000, 3 };

static SVECTOR _gMistShootingGalleryCollision07C38Normals[1] = {
#include "assets/mist_shooting_gallery_collision_07C38_normals.inc"
};

static SVECTOR _gMistShootingGalleryCollision07C38Verts[4] = {
#include "assets/mist_shooting_gallery_collision_07C38_verts.inc"
};

static WorldCollisionGridFace _gMistShootingGalleryCollision07C38Faces[1] = {
#include "assets/mist_shooting_gallery_collision_07C38_faces.inc"
};

static s16 _gMistShootingGalleryCollision07C38Cells[2] = {
#include "assets/mist_shooting_gallery_collision_07C38_cells.inc"
};

#define GRID_CELL(i) (&_gMistShootingGalleryCollision07C38Cells[i])
static s16* _gMistShootingGalleryCollision07C38Table[1] = {
#include "assets/mist_shooting_gallery_collision_07C38_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_shooting_gallery_801851F8 = { NULL, _gMistShootingGalleryCollision07C38Normals, _gMistShootingGalleryCollision07C38Verts, _gMistShootingGalleryCollision07C38Faces, _gMistShootingGalleryCollision07C38Table, 6500, -3680, 1, 1, 4000, 1 };

JukeboxTrack gJukeboxTracksAttach0[3] = {
    { 20, D_mist_shooting_gallery_8017D898 },
    { 23, D_mist_shooting_gallery_8017D884 },
    { 49, D_mist_shooting_gallery_8017D86C },
};

JukeboxTrack gJukeboxTracksAttach1[3] = {
    { 20, D_mist_shooting_gallery_8017D898 },
    { 60, D_mist_shooting_gallery_8017D8C4 },
    { 66, D_mist_shooting_gallery_8017D8A8 },
};

JukeboxTrack gJukeboxTracksAttach2[3] = {
    { 20, D_mist_shooting_gallery_8017D898 },
    { 22, D_mist_shooting_gallery_8017D8F0 },
    { 74, D_mist_shooting_gallery_8017D8DC },
};

JukeboxTrack gJukeboxTracksAttach3[3] = {
    { 67, D_mist_shooting_gallery_8017D92C },
    { 82, D_mist_shooting_gallery_8017D91C },
    { 93, D_mist_shooting_gallery_8017D908 },
};

JukeboxTrack gJukeboxTracksAttach4[3] = {
    { 20, D_mist_shooting_gallery_8017D898 },
    { 21, D_mist_shooting_gallery_8017D954 },
    { 60, D_mist_shooting_gallery_8017D93C },
};

JukeboxTrack gJukeboxTracks0[4] = {
    { 41, D_mist_shooting_gallery_8017D99C },
    { 45, D_mist_shooting_gallery_8017D988 },
    { 58, D_mist_shooting_gallery_8017D97C },
    { 61, D_mist_shooting_gallery_8017D960 },
};

JukeboxTrack gJukeboxTracks1[4] = {
    { 35, D_mist_shooting_gallery_8017D9E0 },
    { 36, D_mist_shooting_gallery_8017D9C8 },
    { 42, D_mist_shooting_gallery_8017D9B8 },
    { 44, D_mist_shooting_gallery_8017D9AC },
};

JukeboxTrack gJukeboxTracks2[4] = {
    { 31, D_mist_shooting_gallery_8017DA20 },
    { 59, D_mist_shooting_gallery_8017DA14 },
    { 89, D_mist_shooting_gallery_8017DA04 },
    { 93, D_mist_shooting_gallery_8017D9F0 },
};

JukeboxTrack gJukeboxTracks3[4] = {
    { 76, D_mist_shooting_gallery_8017DA74 },
    { 77, D_mist_shooting_gallery_8017DA5C },
    { 78, D_mist_shooting_gallery_8017DA48 },
    { 83, D_mist_shooting_gallery_8017DA34 },
};

JukeboxTrack gJukeboxTracks4[4] = {
    { 9, D_mist_shooting_gallery_8017DAC8 },
    { 43, D_mist_shooting_gallery_8017DAB8 },
    { 17, D_mist_shooting_gallery_8017DAA4 },
    { 37, D_mist_shooting_gallery_8017DA88 },
};

UiListRowCallback D_mist_shooting_gallery_80185334[1] = {
    _jukeboxDrawRow,
};

UiList D_mist_shooting_gallery_80185338 = { D_mist_shooting_gallery_80185334, 1, { .unsignedValue = 1 }, 0, 17, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc gJukeboxPanelDesc = { USER_INTERFACE_PANEL_TITLE_STYLE, { -112, -64, 224, 128 }, 48, 0, TASK_BODY_NONE, 192, _mistShootingGalleryJukeboxPanelTask, 0 };

TaskDesc D_mist_shooting_gallery_80185378 = { { { TASK_BODY_NONE, 192 } }, jukeboxHostTask, { .value = 0 } };

TaskDesc D_mist_shooting_gallery_80185384[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_801810D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryClearMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryModeSplashTask, { .value = 0 } },
};

WorldCollisionRoomResources D_mist_shooting_gallery_801853A8[1] = {
    { &D_mist_shooting_gallery_80189968, D_mist_shooting_gallery_8018BDE8, D_mist_shooting_gallery_8018C638, NULL },
};

u8* D_mist_shooting_gallery_801853B8[1] = {
    gViewIdentityMap,
};

ViewCount D_mist_shooting_gallery_801853BC[2] = { 18, 0 };

WorldCoordRoomLighting gMistShootingGalleryRoomLightingTable[1] = {
    {
        .lights       = &gMistShootingGalleryDefaultRoomLights,
        .ambientTable = gMistShootingGalleryViewAmbientTable,
    },
};

DirectionWarpEntry D_mist_shooting_gallery_801853C8[7] = {
    { { { .word = 1024 }, -0x2D50, 0, 5200 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x2D50, 0, 5200 }, { 0, 0, 0, 0 }, 0x51140002, 0x51140001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, -9630, 0, -3040 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -9630, 0, -3040 }, { 0, 0, 0, 0 }, 0x51140004, 0x51140003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6800, 0, 3000 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6800, 0, 3000 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 14, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -8700, 0, -3000 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -8700, 0, -3000 }, { 0, 0, 0, 0 }, 0x51140004, 0x51140003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, -8000, 0, -1800 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -8000, 0, -1800 }, { 0, 0, 0, 0 }, 0x51140006, 0x51140005, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8000, 0, -720 }, { 0, 0, 0, 0 }, { { .word = 0 }, -8000, 0, -720 }, { 0, 0, 0, 0 }, 0x51140006, 0x51140005, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8000, 0, -720 }, { 0, 0, 0, 0 }, { { .word = 0 }, -8000, 0, -720 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 15, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

SVECTOR D_mist_shooting_gallery_80185550[45] = {
    { -11000, -2930, 5580, 0 },
    { -11000, -2930, 4420, 0 },
    { -11000, -2930, 2580, 0 },
    { -11000, -2930, 1420, 0 },
    { -11000, -2930, -420, 0 },
    { -11000, -2930, -1580, 0 },
    { -11080, -2930, -3000, 0 },
    { -9920, -2930, -3000, 0 },
    { -9400, -1990, 5680, 0 },
    { -9400, -1990, 5220, 0 },
    { -9400, -1990, 4480, 0 },
    { -9400, -1990, 4020, 0 },
    { -9400, -1990, 3290, 0 },
    { -9400, -1990, 2820, 0 },
    { -9400, -1990, 2090, 0 },
    { -9400, -1990, 1610, 0 },
    { -9400, -1990, 880, 0 },
    { -9400, -1990, 420, 0 },
    { -9400, -1990, -320, 0 },
    { -9400, -1990, -780, 0 },
    { -8130, -2920, 500, 0 },
    { -6880, -2920, 500, 0 },
    { -8130, -2920, 2750, 0 },
    { -6880, -2920, 2750, 0 },
    { -8130, -2920, 5000, 0 },
    { -6880, -2920, 5000, 0 },
    { -3920, -2000, -410, 0 },
    { -3400, -2000, -560, 0 },
    { -3920, -2000, 6420, 0 },
    { -3400, -2000, 6560, 0 },
    { -11510, -2770, 560, 0 },
    { -11530, -2770, -470, 0 },
    { -11520, -2770, -1500, 0 },
    { -11520, -2770, -2880, 0 },
    { -11270, -2770, -3410, 0 },
    { -9890, -2770, -3420, 0 },
    { -460, -2770, -780, 0 },
    { 5000, -2770, -250, 0 },
    { 7060, -2770, 220, 0 },
    { 12620, -2770, -390, 0 },
    { 12840, -2770, 3000, 0 },
    { 12620, -2770, 6410, 0 },
    { 7060, -2770, 5770, 0 },
    { 4940, -2770, 6280, 0 },
    { -390, -2770, 6800, 0 },
};

static void func_mist_shooting_gallery_8017FC2C(Task* arg0);
static void func_mist_shooting_gallery_8017FD40(Task* task);

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

void mistShootingGalleryPrepareTrainingLoadout(s32 courseLevel)
{
    enum {
        MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM              = 0x40,
        MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM        = 1,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM             = 5,
        MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM        = 6,
        MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX      = 1,
        MIST_SHOOTING_GALLERY_TRAINING_GPS_ATTACHMENT_SLOT   = 2,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT1 = 3,
        MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT2 = 4
    };
    InventoryItemRange* carriedInventory;
    s32                 spellRow;
    s32                 spellColumn;

    carriedInventory = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    // Reset the twelve spell entries; the other six debug entries remain untouched.
    for (spellRow = 0; spellRow < ATTACHMENT_SPELL_COUNT / 3; spellRow++) {
        for (spellColumn = 0; spellColumn < 3; spellColumn++) {
            Gp_DebugAttachLevels[spellColumn + spellRow * 3] = 0;
        }
    }
    Gp_DebugAttachLevels[ATTACHMENT_INDEX_PYROKINESIS] = 1;

    switch (courseLevel) {
        case 1:
        case 2:
            break;
        case 3:
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            break;
        case 4:
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1)->attachSlot  = MIST_SHOOTING_GALLERY_TRAINING_GPS_ATTACHMENT_SLOT;
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1)->attachSlot = MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT1;
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_COLA_ITEM, 1)->attachSlot = MIST_SHOOTING_GALLERY_TRAINING_COLA_ATTACHMENT_SLOT2;
            Gp_DebugAttachLevels[ATTACHMENT_INDEX_ENERGY_SHOT]                                           = 1;
            Gp_DebugAttachLevels[MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX]                        = 1;
            break;
        case 5:
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            itemSetIdentified(MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_GPS_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_RECOVERY1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            inventoryGiveItem(carriedInventory, MIST_SHOOTING_GALLERY_TRAINING_MP_BOOST1_ITEM, 1);
            Gp_DebugAttachLevels[ATTACHMENT_INDEX_ENERGY_SHOT]                    = 1;
            Gp_DebugAttachLevels[MIST_SHOOTING_GALLERY_TRAINING_COMBUSTION_INDEX] = 1;
            break;
    }
    equipmentRestoreHpMp();
    equipmentInitializeWeaponSupplies();
}
void func_mist_shooting_gallery_8017DE7C(UiList* arg0, UiObject* arg1)
{
    s32                               item;
    s32                               i;
    s32                               skip;
    s32                               status;
    s32                               selected;
    s32                               ammo;
    const EquipmentWeaponLoadOptions* row;
    u8*                               weaponIdx;
    InventoryItemRange*               scan;

    item = 0;
    skip = arg0->currentItemIndex;
    i    = 0;
    do {
        if (inventoryIsItemLimitReached(D_mist_shooting_gallery_80184F34[i]) != 0) {
            skip--;
            if (skip < 0) {
                item = D_mist_shooting_gallery_80184F34[i];
                break;
            }
        }
        i++;
    } while (i < 10);

    itemMenuDrawItemRow(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 0);
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
        }
    }
    selected = arg0->rowInputEnabled;
    if (selected == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            scan       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
            weaponIdx  = &gPlayerStatus.weapon;
            row        = &Gp_RelatedQty0.rows[item - EQUIPMENT_WEAPON_ITEM_FIRST];
            ammo       = row->acceptedItemIds[0];
            *weaponIdx = item - 0x7F;
            inventoryResetCarriedRange();
            inventoryClearItems(scan);
            inventoryGiveItem(scan, item, 1);
            inventoryGiveItem(scan, 0x6C, 1);
            equipmentEquipCarriedArmor(0x6C);
            inventoryGiveItem(scan, ammo, 0x3E7)->attachSlot = selected;
            equipmentLoadWeaponConsumable(scan, item, ammo, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
            equipmentRestoreHpMp();
            arg1->result = USER_INTERFACE_RESULT_CONFIRM;
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[45], item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}
static const char D_mist_shooting_gallery_8017D5D8[8]  = "Select";
static const char D_mist_shooting_gallery_8017D5E0[12] = "Red Target";
static const char D_mist_shooting_gallery_8017D5EC[16] = "Brown Target";
static const char D_mist_shooting_gallery_8017D5FC[16] = "Yellow Target";
static const char D_mist_shooting_gallery_8017D60C[8]  = "Woman";
static const char D_mist_shooting_gallery_8017D614[8]  = "Crow";
static const char D_mist_shooting_gallery_8017D61C[4]  = "Bee";
static const char D_mist_shooting_gallery_8017D620[8]  = "Spider";
static const char D_mist_shooting_gallery_8017D628[8]  = "Snake";
static const char D_mist_shooting_gallery_8017D630[12] = "Scorpion";
static const char D_mist_shooting_gallery_8017D63C[4]  = "Rat";
static const char D_mist_shooting_gallery_8017D640[8]  = "Monkey";
static const char D_mist_shooting_gallery_8017D648[8]  = "Bear";
static const char D_mist_shooting_gallery_8017D650[12] = "Bacterium";

/// Updates the centered weapon picker and returns input from its item-help child.
///
/// Borrows the task-owned UI object and singleton list. Initialization identifies
/// and counts owned weapons; the row callback maps displayed rows to that same
/// filtered table. Completed child panels close before input returns to the picker.
static void _mistShootingGalleryWeaponSelectPanelTask(Task* task)
{
    enum { MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS = 10 };
    UiObject* object;
    UiList*   list;
    Task*     child;
    UiObject* childObject;
    s16*      weaponItem;
    s32       weaponIndex;
    s32       ownedWeaponCount;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &D_mist_shooting_gallery_80184F4C;
    uiDrawPanelLabel(&object->panel, D_mist_shooting_gallery_8017D5D8);
    if (task->state == 0) {
        ownedWeaponCount = 0;
        weaponIndex      = ownedWeaponCount;
        weaponItem       = D_mist_shooting_gallery_80184F34;
        do {
            if (inventoryIsItemLimitReached(*weaponItem) != 0) {
                itemSetIdentified(*weaponItem, 1);
                ownedWeaponCount += 1;
            }
            weaponIndex++;
            weaponItem++;
        } while (weaponIndex < (s32)ARRAY_SIZE(D_mist_shooting_gallery_80184F34));

        list->itemCount = ownedWeaponCount;
        if ((u8)ownedWeaponCount > MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_WEAPON_MAX_VISIBLE_ROWS;
        } else {
            list->visibleRowCount.unsignedValue = ownedWeaponCount;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        object->panel.bounds.unsignedRect.x = -((s16)object->panel.bounds.unsignedRect.w / 2);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        task->state                        += 1;
    }
    uiUpdateList(list, &object->panel);
    child = task->firstChild;
    if (child != NULL) {
        childObject = child->spawnArg2.pointer;
        if (childObject->result == USER_INTERFACE_RESULT_CANCEL || childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(childObject, childObject->owner);
            object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
        }
    }
}
static const char D_mist_shooting_gallery_8017D65C[] = "TOTAL SCORE";

void func_mist_shooting_gallery_8017E234(Task* task)
{
    u8                       buf[0x20];
    TextDrawReq              req1;
    TextDrawReq              req2;
    TextDrawReq              req3;
    TextDrawReq              req4;
    TextDrawReq              req5;
    TextDrawReq              req6;
    MistShootingGalleryWork* work;
    s32                      rows;
    s32                      total;
    UiObject*                obj;
    s32                      i;
    s32                      kills;
    s32                      points;
    s32                      subtotal;
    s32                      xOff;
    s32                      y;
    s32                      status;
    s32                      state;
    s32                      bonus;
    s32                      flag;
    UiObject*                childObj;
    s32                      result;
    s32                      bottom1;
    s32                      bottom2;
    Task*                    child;

    i     = 0;
    rows  = 0;
    total = 0;
    obj   = task->spawnArg2.pointer;
    work  = D_mist_shooting_gallery_8018E0C4->work;
    xOff  = obj->panel.contentLeft.signedValue + 2;
    y     = obj->panel.contentTop.signedValue + 0x17;
    do {
        kills = work->kills[i];
        if (kills > 0) {
            points = D_mist_shooting_gallery_80184F98[i].points;
            rows  += 1;

            req1.x          = obj->panel.contentOriginX.unsignedValue + xOff;
            req1.y          = obj->panel.contentOriginY.unsignedValue + y;
            req1.otIndex    = obj->panel.otIndex.signedValue + 1;
            req1.colorRgb   = 0x606060;
            req1.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req1.alignment  = TEXT_ALIGNMENT_LEFT;
            req1.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&req1, D_mist_shooting_gallery_80184F98[i].name);

            req2.x          = obj->panel.contentOriginX.unsignedValue + 0x6E + xOff;
            req2.y          = obj->panel.contentOriginY.unsignedValue + y;
            req2.otIndex    = obj->panel.otIndex.signedValue + 1;
            req2.colorRgb   = 0x606060;
            req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req2.alignment  = TEXT_ALIGNMENT_RIGHT;
            req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&req2, textItoaSigned(buf, points));

            req3.x          = obj->panel.contentOriginX.unsignedValue + 0x91 + xOff;
            req3.y          = obj->panel.contentOriginY.unsignedValue + y;
            req3.otIndex    = obj->panel.otIndex.signedValue + 1;
            req3.colorRgb   = 0x606060;
            req3.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req3.alignment  = TEXT_ALIGNMENT_RIGHT;
            req3.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&req3, textItoaSigned(buf, kills));
            subtotal = kills * points;

            req4.x          = obj->panel.contentOriginX.unsignedValue - 5 - xOff;
            req4.y          = obj->panel.contentOriginY.unsignedValue + y;
            req4.otIndex    = obj->panel.otIndex.signedValue + 1;
            req4.colorRgb   = 0x606060;
            req4.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req4.alignment  = TEXT_ALIGNMENT_RIGHT;
            req4.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&req4, textItoaSigned(buf, subtotal));
            total += subtotal;
            y     += 0xB;
        }
        i += 1;
    } while (i < MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT);

    uiDrawHorizontalSeparator(&(obj)->panel, xOff, -xOff, obj->panel.contentBottom.signedValue - 0xE);

    req1.x          = obj->panel.contentOriginX.unsignedValue + 0x78 + xOff;
    bottom1         = obj->panel.contentOriginY.unsignedValue - 6;
    req1.y          = obj->panel.contentBottom.unsignedValue + bottom1;
    req1.otIndex    = obj->panel.otIndex.signedValue + 1;
    req1.colorRgb   = 0x606060;
    req1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req1.alignment  = TEXT_ALIGNMENT_RIGHT;
    req1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req1, D_mist_shooting_gallery_8017D65C);

    req2.x          = obj->panel.contentOriginX.unsignedValue - 5 - xOff;
    bottom2         = obj->panel.contentOriginY.unsignedValue - 4;
    req2.y          = obj->panel.contentBottom.unsignedValue + bottom2;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = 0x606060;
    req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req2.alignment  = TEXT_ALIGNMENT_RIGHT;
    req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req2, textItoaSigned(buf, total));

    uiDrawHorizontalSeparator(&(obj)->panel, xOff, -xOff, obj->panel.contentTop.signedValue + 0xA);

    y               = obj->panel.contentTop.signedValue + 6;
    req3.x          = obj->panel.contentOriginX.unsignedValue + 0x1E + xOff;
    req3.y          = obj->panel.contentOriginY.unsignedValue + y;
    req3.otIndex    = obj->panel.otIndex.signedValue + 1;
    req3.colorRgb   = 0x606060;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_CENTER;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, "NMC");

    req4.x          = obj->panel.contentOriginX.unsignedValue + 0x73 + xOff;
    req4.y          = obj->panel.contentOriginY.unsignedValue + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = 0x606060;
    req4.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req4.alignment  = TEXT_ALIGNMENT_RIGHT;
    req4.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req4, "SCORE");

    req5.x          = obj->panel.contentOriginX.unsignedValue + 0x96 + xOff;
    req5.y          = obj->panel.contentOriginY.unsignedValue + y;
    req5.otIndex    = obj->panel.otIndex.signedValue + 1;
    req5.colorRgb   = 0x606060;
    req5.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req5.alignment  = TEXT_ALIGNMENT_RIGHT;
    req5.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req5, "KILL");

    req6.x          = obj->panel.contentOriginX.unsignedValue - xOff;
    req6.y          = obj->panel.contentOriginY.unsignedValue + y;
    req6.otIndex    = obj->panel.otIndex.signedValue + 1;
    req6.colorRgb   = 0x606060;
    req6.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req6.alignment  = TEXT_ALIGNMENT_RIGHT;
    req6.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req6, "TOTAL");

    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, "Result");

    if (task->state == 0) {
        if (gGameSession->battleResetPending == 1) {
            uiStartPanelHiding(obj, obj->owner);
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            task->state = 0x100;
            return;
        }
        gGameSession->battleResetPending = 1;
        uiSetPanelContentSize(&(obj)->panel, 0, (rows * 0xB) + 0x21);
        obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
        task->state                      = task->state + 1;
    }

    status = obj->panel.control.word;
    if ((status == 1) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        bonus = mistShootingGalleryGetBonusBp(total);
        state = task->state;
        if (state == status) {
            if (bonus > 0) {
                flag = work->course + 0x125;
                if (gameFlagGetNibble(flag) == 0) {
                    if (func_mist_shooting_gallery_80184970(bonus) == state) {
                        gameFlagSetNibble(flag, 2);
                    } else {
                        gameFlagSetNibble(flag, 1);
                    }
                    uiSpawnObject(&D_mist_shooting_gallery_8018501C, total, 1, 1, obj);
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                } else {
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            } else {
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        } else {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        result   = childObj->result;
        if (result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->result = result;
        }
    }
}

void func_mist_shooting_gallery_8017E854(Task* task)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    UiObject*     obj;
    s32           score;
    s32           bonus;
    s32           total;
    s32           xOff;
    s32           top;
    s32           y;
    s32           color;
    PlayerStatus* cfg;

    obj   = task->spawnArg2.pointer;
    score = task->spawnArg1.value;

    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, "BONUS");
    if (task->state == 0) {
        bonus = mistShootingGalleryGetBonusBp(score);
        cfg   = &gPlayerStatus;
        if (bonus > 0) {
            total   = cfg->bp + bonus;
            cfg->bp = total;
            if (total > 999999) {
                cfg->bp = 999999;
            }
        }
        task->state = task->state + 1;
    }

    color = 0x606060;
    xOff  = obj->panel.contentLeft.signedValue + 2;
    top   = obj->panel.contentTop.signedValue;
    y     = top + 0xB;

    req1.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req1.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req1.otIndex    = obj->panel.otIndex.signedValue + 1;
    req1.colorRgb   = color;
    req1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req1.alignment  = TEXT_ALIGNMENT_LEFT;
    req1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req1, D_mist_shooting_gallery_8017D65C);

    req2.x          = obj->panel.contentOriginX.unsignedValue - xOff;
    req2.y          = obj->panel.contentOriginY.unsignedValue + y;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req2.alignment  = TEXT_ALIGNMENT_RIGHT;
    req2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req2, textItoaSigned(buf, score));

    uiDrawHorizontalSeparator(&(obj)->panel, xOff, -xOff, top + 0x1B);

    y               = top + 0x25;
    req3.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req3.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req3.otIndex    = obj->panel.otIndex.signedValue + 1;
    req3.colorRgb   = color;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_LEFT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, "BONUS BP");

    req4.x          = obj->panel.contentOriginX.unsignedValue - xOff;
    req4.y          = obj->panel.contentOriginY.unsignedValue + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = 0x37A78;
    req4.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req4.alignment  = TEXT_ALIGNMENT_RIGHT;
    req4.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req4, textItoaSigned(buf, mistShootingGalleryGetBonusBp(score)));

    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
static const char D_mist_shooting_gallery_8017D6A0[] = "Replay Mode";

static const char D_mist_shooting_gallery_8017D6AC[] = "Bounty Mode";

static const char D_mist_shooting_gallery_8017D6B8[] = "Scavenger Mode";

static const char D_mist_shooting_gallery_8017D6C8[] = "Nightmare Mode";

static const _MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D6D8 = { { D_mist_shooting_gallery_8017D6A0, D_mist_shooting_gallery_8017D6AC, D_mist_shooting_gallery_8017D6B8, D_mist_shooting_gallery_8017D6C8 } };

void func_mist_shooting_gallery_8017EAE0(Task* task)
{
    UiObject* obj  = task->spawnArg2.pointer;
    UiList*   list = &D_mist_shooting_gallery_8018503C;

    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, "SELECT");
    if (task->state == 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank == 0) {
            list->itemCount                     = 2;
            list->visibleRowCount.unsignedValue = 2;
        } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.replayRank < 2) {
            list->itemCount                     = 3;
            list->visibleRowCount.unsignedValue = 3;
        } else {
            list->itemCount                     = 4;
            list->visibleRowCount.unsignedValue = 4;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1) {
            list->itemCount                     = 4;
            list->visibleRowCount.unsignedValue = 4;
        }
        list->selectedItemIndex = 0;
        uiFitPanelToList(list, &(obj)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSpawnObject(&D_mist_shooting_gallery_8018507C[0], 0, 0, 1, obj);
        uiSpawnObject(&D_mist_shooting_gallery_8018507C[1], 0, 0, 1, obj);
        uiSpawnObject(&D_mist_shooting_gallery_8018507C[2], 0, 0, 1, obj);
        task->state = task->state + 1;
    }
    uiUpdateList(list, &obj->panel);
    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
/// Converts an accumulated EXP or BP total to the selected run mode's carryover.
///
/// Reads the live save's mode: Replay divides by 10, Bounty by 20, Scavenger
/// by 100, and Nightmare returns zero. Other mode values use Replay's divisor.
/// Signed division truncates toward zero; only the upper limit of 999999 is
/// clamped. The STATUS panel and the closing sequence use the same result.
static inline s32 _mistShootingGalleryScaleReward(s32 unscaledTotal)
{
    enum {
        MIST_SHOOTING_GALLERY_REWARD_MODE_BOUNTY       = 1,
        MIST_SHOOTING_GALLERY_REWARD_MODE_SCAVENGER    = 2,
        MIST_SHOOTING_GALLERY_REWARD_MODE_NIGHTMARE    = 3,
        MIST_SHOOTING_GALLERY_REPLAY_REWARD_DIVISOR    = 10,
        MIST_SHOOTING_GALLERY_BOUNTY_REWARD_DIVISOR    = 20,
        MIST_SHOOTING_GALLERY_SCAVENGER_REWARD_DIVISOR = 100,
        MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX     = 999999
    };
    s32 reward;

    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case MIST_SHOOTING_GALLERY_REWARD_MODE_NIGHTMARE:
            return 0;
        case MIST_SHOOTING_GALLERY_REWARD_MODE_SCAVENGER:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_SCAVENGER_REWARD_DIVISOR;
            break;
        case MIST_SHOOTING_GALLERY_REWARD_MODE_BOUNTY:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_BOUNTY_REWARD_DIVISOR;
            break;
        default:
            reward = unscaledTotal / MIST_SHOOTING_GALLERY_REPLAY_REWARD_DIVISOR;
            break;
    }
    if (reward > MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX) {
        reward = MIST_SHOOTING_GALLERY_CARRYOVER_REWARD_MAX;
    }
    return reward;
}

/// Draws the selected run mode's starting HP/MP and scaled EXP/BP carryover.
///
/// Borrows the task-owned UI object. The live mode must be 0..3; HP below the
/// normal starting 100 or MP below 30 uses the warning colour. EXP/BP use the
/// captured totals, rather than the player totals being changed by the session.
static void _mistShootingGalleryModeStatusPanelTask(Task* task)
{
    enum { MIST_SHOOTING_GALLERY_BASE_HP_WARNING_THRESHOLD = 100,
           MIST_SHOOTING_GALLERY_BASE_MP_WARNING_THRESHOLD = 30 };
    u8          numberText[0x20];
    TextDrawReq hpCaption;
    TextDrawReq mpCaption;
    TextDrawReq expCaption;
    TextDrawReq bpCaption;
    UiObject*   object;
    s32         displayedValue;
    s32         valueColor;
    s32         captionX;
    s16         contentTop;
    s32         rowY;

    // Draws a STATUS caption, capturing object, captionX and rowY. The request
    // must be a plain local lvalue; the text argument is evaluated once.
#define MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(caption, captionText)                      \
    {                                                                                        \
        (caption).x          = object->panel.contentOriginX.unsignedValue + captionX;        \
        (caption).y          = (s16)(object->panel.contentOriginY.unsignedValue - 8) + rowY; \
        (caption).otIndex    = object->panel.otIndex.signedValue + 1;                        \
        (caption).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                        \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                       \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                          \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                           \
        textDrawString(&(caption), captionText);                                             \
    }

    object = task->spawnArg2.pointer;
    uiDrawTitle(&(object)->panel, "STATUS");
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(4));
        task->state = task->state + 1;
    }

    valueColor     = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;
    contentTop     = object->panel.contentTop.signedValue;
    rowY           = contentTop + 0xF;
    displayedValue = Gp_StatRows[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseHp.hpWord;
    captionX       = object->panel.contentLeft.signedValue + 6;
    if (displayedValue < MIST_SHOOTING_GALLERY_BASE_HP_WARNING_THRESHOLD) {
        valueColor = MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR;
    }

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(hpCaption, "HP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x1E;
    displayedValue = Gp_StatRows[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseMp;
    valueColor     = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;
    if (displayedValue < MIST_SHOOTING_GALLERY_BASE_MP_WARNING_THRESHOLD) {
        valueColor = MIST_SHOOTING_GALLERY_MODE_WARNING_TEXT_COLOR;
    }

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(mpCaption, "MP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), valueColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY           = contentTop + 0x2D;
    displayedValue = _mistShootingGalleryScaleReward(D_mist_shooting_gallery_8018E0BC);

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(expCaption, "EXP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    rowY          += 0xF;
    displayedValue = _mistShootingGalleryScaleReward(D_mist_shooting_gallery_8018E0C0);

    MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION(bpCaption, "BP");
    textDrawUiLine(object, -captionX, rowY, textItoaSigned(numberText, displayedValue), MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
#undef MIST_SHOOTING_GALLERY_DRAW_STATUS_CAPTION
}
static const _MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D708 = { { D_mist_shooting_gallery_80184DD4, D_mist_shooting_gallery_80184E24, D_mist_shooting_gallery_80184E70, D_mist_shooting_gallery_80184EC4 } };

/// Draws the selected run mode's mission, condition, enemy and supply ratings.
///
/// Borrows the task-owned UI object and reads the live mode in 0..3. The local
/// tables supply levels 1..5 and their names; each gauge contains that many marks.
static void _mistShootingGalleryModeDataPanelTask(Task* task)
{
    UiObject* object = task->spawnArg2.pointer;
    // Each DATA row's rating for every run mode, indexed by the save's gameMode.
    _MistShootingGalleryRating missionLevels[4] = {
        { 2, "EASY" },
        { 3, "NORMAL" },
        { 4, "HARD" },
        { 5, "VERY HARD" },
    };
    _MistShootingGalleryRating conditions[4] = {
        { 5, "GOOD" },
        { 5, "GOOD" },
        { 4, "EXHAUSTED" },
        { 1, "SICK" },
    };
    _MistShootingGalleryRating enemyLevels[4] = {
        { 2, "EASY" },
        { 4, "STRONG" },
        { 3, "NORMAL" },
        { 5, "VERY STRONG" },
    };
    _MistShootingGalleryRating supplyLevels[4] = {
        { 4, "RICH" },
        { 3, "NORMAL" },
        { 1, "VERY POOR" },
        { 2, "POOR" },
    };
    // The gauge drawn for each rating level: entry n is n gauge marks. No
    // rating has level 0, whose entry repeats the single mark.
    const char* gaugeByLevel[6] = {
        D_mist_shooting_gallery_80184F18,
        D_mist_shooting_gallery_80184F18,
        D_mist_shooting_gallery_80184F1C,
        D_mist_shooting_gallery_80184F20,
        D_mist_shooting_gallery_80184F24,
        D_mist_shooting_gallery_80184F2C,
    };
    TextDrawReq                       missionCaption;
    TextDrawReq                       missionRatingText;
    TextDrawReq                       conditionCaption;
    TextDrawReq                       conditionRatingText;
    TextDrawReq                       enemyCaption;
    TextDrawReq                       enemyRatingText;
    TextDrawReq                       supplyCaption;
    TextDrawReq                       supplyRatingText;
    const _MistShootingGalleryRating* rating;
    s32                               contentLeft;
    s32                               contentTop;
    s32                               captionX;
    s32                               rowY;

    // Finishes a positioned caption and draws its rating word and gauge.
    // Captures object, rowY, rating and gaugeByLevel. Requests must be plain
    // local lvalues, ratings a four-entry array and captionText readable text.
    // The table and text are evaluated once; request lvalues occur repeatedly.
#define MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(caption, ratingText, ratings, captionText)                                                                                       \
    {                                                                                                                                                                           \
        (caption).otIndex    = object->panel.otIndex.signedValue + 1;                                                                                                           \
        rating               = &(ratings)[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];                                                                                   \
        (caption).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                                                                                                           \
        (caption).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                                                                                                          \
        (caption).alignment  = TEXT_ALIGNMENT_LEFT;                                                                                                                             \
        (caption).drawMode   = TEXT_DRAW_OUTLINED;                                                                                                                              \
        textDrawString(&(caption), captionText);                                                                                                                                \
                                                                                                                                                                                \
        (ratingText).x          = object->panel.contentOriginX.unsignedValue + 0x41;                                                                                            \
        (ratingText).y          = (s16)(object->panel.contentOriginY.unsignedValue - 3) + rowY;                                                                                 \
        (ratingText).otIndex    = object->panel.otIndex.signedValue + 1;                                                                                                        \
        (ratingText).colorRgb   = MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR;                                                                                                        \
        (ratingText).glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                                                                                      \
        (ratingText).alignment  = TEXT_ALIGNMENT_RIGHT;                                                                                                                         \
        (ratingText).drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;                                                                                                               \
        textDrawString(&(ratingText), rating->name);                                                                                                                            \
        textDrawUiLine(object, 0x46, rowY, (const u8*)gaugeByLevel[rating->level], MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT); \
    }

    uiDrawTitle(&(object)->panel, D_mist_shooting_gallery_8017D820);

    contentLeft      = object->panel.contentLeft.signedValue;
    object->result   = USER_INTERFACE_RESULT_NONE;
    captionX         = contentLeft + 0xB;
    contentTop       = object->panel.contentTop.signedValue;
    missionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    rowY             = contentTop + 0xB;
    missionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(missionCaption, missionRatingText, missionLevels, D_mist_shooting_gallery_8017D828);
    uiDrawHorizontalSeparator(&(object)->panel, contentLeft + 6, -captionX + 5, contentTop + 0xD);

    rowY               = contentTop + 0x1E;
    conditionCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    conditionCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(conditionCaption, conditionRatingText, conditions, D_mist_shooting_gallery_8017D838);

    rowY           = contentTop + 0x2D;
    enemyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    enemyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(enemyCaption, enemyRatingText, enemyLevels, D_mist_shooting_gallery_8017D844);

    rowY            = contentTop + 0x3C;
    supplyCaption.x = object->panel.contentOriginX.unsignedValue + captionX;
    supplyCaption.y = (s16)(object->panel.contentOriginY.unsignedValue - 6) + rowY;
    MIST_SHOOTING_GALLERY_DRAW_MODE_RATING(supplyCaption, supplyRatingText, supplyLevels, D_mist_shooting_gallery_8017D850);
#undef MIST_SHOOTING_GALLERY_DRAW_MODE_RATING
}
/// Task handler for the gallery's closing sequence. State 0 spawns the results
/// panel and stashes `gPlayerStatus.exp` / `gPlayerStatus.bp` in
/// `D_mist_shooting_gallery_8018E0BC` / `_8018E0C0`.
/// State 1 waits for the panel to confirm (`result == USER_INTERFACE_RESULT_CONFIRM`), then writes both
/// totals back scaled down by the bonus mode - the same divisor table as
/// `_mistShootingGalleryScaleReward`, clamped to 999999. Once the kill
/// countdown runs out the task exits and the stage is flagged as ended.
void func_mist_shooting_gallery_8017F6C8(Task* task)
{
    UiObject*     obj;
    PlayerStatus* cfg = &gPlayerStatus;

    if (task->state == 0) {
        obj = uiSpawnObject(&D_mist_shooting_gallery_80185060, 0, 1, 1, NULL);
        if (obj != NULL) {
            D_mist_shooting_gallery_8018E0C0 = cfg->bp;
            D_mist_shooting_gallery_8018E0BC = cfg->exp;
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            task->spawnArg2.pointer = obj;
            task->state             = task->state + 1;
        }
    } else if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->result == USER_INTERFACE_RESULT_CONFIRM) {
            task->killCountdown = 0xA;
            uiStartTreeClosing(obj, obj->owner);
            equipmentRecalculateMaxHp();
            equipmentRecalculateMaxMp();

            cfg->exp = _mistShootingGalleryScaleReward(D_mist_shooting_gallery_8018E0BC);
            cfg->bp  = _mistShootingGalleryScaleReward(D_mist_shooting_gallery_8018E0C0);
            equipmentRestoreHpMp();
            task->state = task->state + 1;
        }
    } else {
        task->killCountdown = task->killCountdown - 1;
        if (task->killCountdown < 0) {
            taskCallExit(task);
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            Wip_UiHolder = NULL;
            stageRequestModeTaskExit();
        }
    }
}
s32 mistShootingGalleryOpenWeaponMenu(s32 unused)
{
    enum { MIST_SHOOTING_GALLERY_WEAPON_MENU_REQUEST = 0x44 };

    displayQueueModeTask(&D_mist_shooting_gallery_80184F8C, MIST_SHOOTING_GALLERY_WEAPON_MENU_REQUEST, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Draws a run-mode row and selects its mode whenever the row has input.
///
/// The borrowed list's current row must be 0..3. Selection writes the live save
/// immediately as the cursor moves; the enclosing panel handles confirmation.
static void _mistShootingGalleryModeRow(UiList* list, UiObject* object)
{
    _MistShootingGalleryModeTexts modeNames;

    modeNames = D_mist_shooting_gallery_8017D6D8;
    // The list has one row per run mode, so the row index is the mode.
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue - 1, (const u8*)modeNames.byMode[list->currentItemIndex], list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode = list->currentItemIndex;
    }
}
/// Retained out-of-line copy of the EXP/BP carryover conversion.
///
/// Uses `_mistShootingGalleryScaleReward`'s contract. Nothing calls this copy;
/// its instructions remain part of the room overlay.
static s32 _mistShootingGalleryScaleRewardOutOfLine(s32 unscaledTotal)
{
    return _mistShootingGalleryScaleReward(unscaledTotal);
}
/// Draws the three-line description of the selected run mode below its menu.
///
/// Borrows the task-owned UI object. The live mode must be 0..3; initialization
/// sizes and anchors the panel, and every tick reflects the mode row's selection.
static void _mistShootingGalleryModeHelpPanelTask(Task* task)
{
    UiObject*                     object           = task->spawnArg2.pointer;
    _MistShootingGalleryModeTexts modeDescriptions = D_mist_shooting_gallery_8017D708;

    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(3) + 1);
        object->panel.bounds.unsignedRect.y = 0x68 - object->panel.bounds.unsignedRect.h;
        task->state                         = task->state + 1;
    }
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, (const u8*)modeDescriptions.byMode[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode], MIST_SHOOTING_GALLERY_MODE_TEXT_COLOR, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}
void mistShootingGalleryOpenCarryoverModeMenu(void)
{
    enum { MIST_SHOOTING_GALLERY_CARRYOVER_ENTRY_WARP = 7 };

    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) && (gGameSession->location.loc.warp == MIST_SHOOTING_GALLERY_CARRYOVER_ENTRY_WARP)) {
        displayQueueModeTask(&D_mist_shooting_gallery_801850D0, 0, 0, STAGE_ENTRY_RELOAD);
    }
}
static void func_mist_shooting_gallery_8017FC2C(Task* arg0)
{
    s32 var_a0;

    arg0->msgTable = D_mist_shooting_gallery_801850E8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    func_actor_215100_8014C5E0(0x340, 0, 2);
    if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
        sceneSetPlacedActorDrawMode(1, 0);
        var_a0 = 1;
    } else {
        var_a0 = 0;
    }
    _mistShootingGallerySetStoryBarrierLowered(var_a0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 7) {
        taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 0, 0, 0);
    } else if (gGameSession->location.loc.warp == 7) {
        taskSpawnFromTable(D_actor_215100_8014E13C, 0, 0, 0);
    }
    if ((gGameSession->location.loc.warp == 6) && (gameFlagGetNibble(GAME_FLAG_0ED) != 0)) {
        capRunCommandWithTransition(0x16);
    }
    gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_AREA_MUSIC;
    arg0->state             = arg0->state + 1;
}

static void func_mist_shooting_gallery_8017FD40(Task* task)
{
    u8 temp_v1;

    if ((gGameSession->location.loc.variant == 1) && (gGameSession->eventState == 0)) {
        temp_v1 = gGameSession->location.loc.view;
        if ((temp_v1 == 3) || (temp_v1 == 9) || (temp_v1 == 0x12)) {
            sceneSetPlacedActorDrawMode(1, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_0ED) == 0) {
            sceneSetPlacedActorDrawMode(1, 1);
        }
    }
    func_actor_215100_8014A398();
}
void func_mist_shooting_gallery_8017FDD0(Task* task)
{
    s16 texturePageX;

    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            // Select the relocated CAP file and its VRAM texture-page origin.
            if (task->spawnArg2.value == MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS) {
                capSelectLoadedFile(MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_HIGH_COMMANDS;
            } else {
                capSelectLoadedFile(MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_LOW_COMMANDS;
            }
            capSetTexturePage(texturePageX, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state += 1;
            return;
        case 1:
            if (capIsBusy() != 0) {
                return;
            }
            task->state += 1;
            return;
        case 2:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

static const char D_mist_shooting_gallery_8017D820[] = "DATA";

static const char D_mist_shooting_gallery_8017D828[] = "MISSION LEVEL";

static const char D_mist_shooting_gallery_8017D838[] = "CONDITION";

static const char D_mist_shooting_gallery_8017D844[] = "ENEMY LEVEL";

/// "SUPPLY LEVEL", followed by the non-zero padding the original toolchain left.
static const char D_mist_shooting_gallery_8017D850[16] = "SUPPLY LEVEL\0\xD0\x0E\xF0";

/// Rejects every key-item use request without changing room or inventory state.
///
/// Handles `MIST_SHOOTING_GALLERY_MESSAGE_USE_KEY_ITEM`; all arguments are
/// ignored. Returns zero, which makes the item menu show its unavailable-use
/// notice. The receiver and its message table must belong to this loaded room.
static s32 _mistShootingGalleryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { MIST_SHOOTING_GALLERY_KEY_ITEM_UNUSABLE = 0 };

    return MIST_SHOOTING_GALLERY_KEY_ITEM_UNUSABLE;
}

s32 func_mist_shooting_gallery_8017FEB8(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    if (src->areaId == GAME_AREA_MIST_PARKING && src->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) != 0) {
            dst->room += 2;
        }
    }
    if (src->areaId == GAME_AREA_MIST_SHOOTING_GALLERY) {
        if (dst->warp == 5 && func_actor_215100_8014AA54(src) == 2) {
            return 2;
        }
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            if (dst->warp == 6) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
                gPlayerStatus.resourceVariant                       = 4;
                gGameSession->hideHud                               = 1;
                inventoryInitializeShootingGalleryLoadout();
            }
            if (dst->warp == 5) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 1;
                gPlayerStatus.resourceVariant                       = 3;
                gGameSession->hideHud                               = 1;
                inventoryRestoreCarriedLoadout();
            }
        }
    }
    return 1;
}

s32 func_mist_shooting_gallery_80180000(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 >= 5) {
        if (arg2 >= 9) {
            if (arg2 < 0x23) {
                if (arg2 >= 0x21) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                }
            }
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
        }
    }
    return 0;
}

s32 func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if ((request->actionId == 1) && (D_actor_215100_8014D038 == 0)) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(D_actor_215100_8014E13C, 1, 1, 0);
        D_80114D08 = 0xA;
    }
    if ((request->actionId == 2) && (gameFlagGetNibble(GAME_FLAG_0ED) == 0)) {
        actor215100StartPierceConversation();
    }
    if (request->actionId == 3) {
        func_actor_215100_8014AB6C();
    }
    if ((request->actionId == 4) && (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN) == 0)) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x3B);
        gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN, 1);
        evsStartScriptWithSkip(D_actor_215100_80153274, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_215100_80153D6C);
    }
    return 0;
}

/// The room task's three-state table, run from a stack copy by
/// `mistShootingGalleryRoomTask`: the entry tick
/// `func_mist_shooting_gallery_8017FC2C`, the per-frame state
/// `func_mist_shooting_gallery_8017FD40`, then `taskKill`.
static const TaskFuncTable3 D_mist_shooting_gallery_8017D860 = {
    { func_mist_shooting_gallery_8017FC2C, func_mist_shooting_gallery_8017FD40, taskKill },
};

/// The jukebox's track names ("1. Crazy King", ...), reached only through the
/// track lists in `.data`.
/// Numbered names the room lists. Nothing in the code refers to them: the
/// room's data holds them in records that pair each with a small number.
const char D_mist_shooting_gallery_8017D86C[] = "3. Heaven-sent Killer";
const char D_mist_shooting_gallery_8017D884[] = "2. Eager For Blood";
const char D_mist_shooting_gallery_8017D898[] = "1. Crazy King";
const char D_mist_shooting_gallery_8017D8A8[] = "3. Crawling Waste Emperor";
const char D_mist_shooting_gallery_8017D8C4[] = "2. Pick Up The Gauntlet";
const char D_mist_shooting_gallery_8017D8DC[] = "3. Hunter's Moon";
const char D_mist_shooting_gallery_8017D8F0[] = "2. Quadrumanous Leader";
const char D_mist_shooting_gallery_8017D908[] = "3. Genic Reactor";
const char D_mist_shooting_gallery_8017D91C[] = "2. Rebel Forces";
const char D_mist_shooting_gallery_8017D92C[] = "1. Yellow Rain";
const char D_mist_shooting_gallery_8017D93C[] = "3. Pick Up The Gauntlet";
const char D_mist_shooting_gallery_8017D954[] = "2. Ambush!";
const char D_mist_shooting_gallery_8017D960[] = "4. Dark Voice Of The Heart";
const char D_mist_shooting_gallery_8017D97C[] = "3. Requiem";
const char D_mist_shooting_gallery_8017D988[] = "2. Rock And Fire";
const char D_mist_shooting_gallery_8017D99C[] = "1. Ghost Town";
const char D_mist_shooting_gallery_8017D9AC[] = "4. Snooper";
const char D_mist_shooting_gallery_8017D9B8[] = "3. Wild Hunt";
const char D_mist_shooting_gallery_8017D9C8[] = "2. Lightning Operation";
const char D_mist_shooting_gallery_8017D9E0[] = "1. L.A. Maze";
const char D_mist_shooting_gallery_8017D9F0[] = "4. Genic Reactor";
const char D_mist_shooting_gallery_8017DA04[] = "3. Mental Agony";
const char D_mist_shooting_gallery_8017DA14[] = "2. Wishwash";
const char D_mist_shooting_gallery_8017DA20[] = "1. Curse Your Fate!";
const char D_mist_shooting_gallery_8017DA34[] = "4. Killing Field";
const char D_mist_shooting_gallery_8017DA48[] = "3. Fool's Paradise";
const char D_mist_shooting_gallery_8017DA5C[] = "2. Gazing into The Void";
const char D_mist_shooting_gallery_8017DA74[] = "1. Man Made Nature";
const char D_mist_shooting_gallery_8017DA88[] = "4. Lazing Away the Morning";
const char D_mist_shooting_gallery_8017DAA4[] = "3. Out Of Phase 2";
const char D_mist_shooting_gallery_8017DAB8[] = "2. The Vagrants";
const char D_mist_shooting_gallery_8017DAC8[] = "1. Tower Rendezvous";

void mistShootingGalleryRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_mist_shooting_gallery_8017D860;
    states.funcs[task->state](task);
}

/// Restores the story-controlled barrier, then lowers it by 3000 room Y units.
///
/// Zero leaves the template position; nonzero lowers it below the floor.
/// Borrows the loaded collision grid's first three normals/faces and eight
/// vertices. The entry handler chooses the lowered position after the story flag.
static void _mistShootingGallerySetStoryBarrierLowered(s32 lowered)
{
    enum { MIST_SHOOTING_GALLERY_STORY_BARRIER_FACE_COUNT   = 3,
           MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT = 8,
           MIST_SHOOTING_GALLERY_STORY_BARRIER_LOWER_Y      = 3000 };
    WorldCollisionGrid* liveGrid     = &D_mist_shooting_gallery_80189968;
    WorldCollisionGrid* templateGrid = &D_mist_shooting_gallery_80185198;
    SVECTOR             offset;
    s32                 index;

    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_FACE_COUNT; index++) {
        liveGrid->normals[index].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index].vz = templateGrid->normals[index].vz;
        liveGrid->faces[index]      = templateGrid->faces[index];
    }
    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = MIST_SHOOTING_GALLERY_STORY_BARRIER_LOWER_Y;
    }
    offset.vz = 0;
    for (index = 0; index < MIST_SHOOTING_GALLERY_STORY_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index].vx += offset.vx;
        liveGrid->vertices[index].vy += offset.vy;
        liveGrid->vertices[index].vz += offset.vz;
    }
}

void mistShootingGallerySetTrainingBarrierLowered(s32 lowered)
{
    enum { MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET = 3,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET = 8,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_COUNT  = 4,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_OFFSET_COUNT  = 8,
           MIST_SHOOTING_GALLERY_TRAINING_BARRIER_LOWER_Y       = 4000 };
    WorldCollisionGrid*     liveGrid     = &D_mist_shooting_gallery_80189968;
    WorldCollisionGrid*     templateGrid = &D_mist_shooting_gallery_801851F8;
    WorldCollisionGridFace* liveFace     = &D_mist_shooting_gallery_80189968.faces[MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET];
    WorldCollisionGridFace* templateFace = D_mist_shooting_gallery_801851F8.faces;
    SVECTOR                 offset;
    s32                     index;
    s32                     corner;

    for (index = 0; index < 1; index++) {
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vx = templateGrid->normals[index].vx;
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vy = templateGrid->normals[index].vy;
        liveGrid->normals[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET].vz = templateGrid->normals[index].vz;
        for (corner = 0; corner < ARRAY_SIZE(liveFace->vertexIndices); corner++) {
            liveFace->vertexIndices[corner] = templateFace->vertexIndices[corner] + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET;
        }
        liveFace->normalIndex  = templateFace->normalIndex + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_NORMAL_OFFSET;
        liveFace->surfaceClass = templateFace->surfaceClass;
        liveFace++;
        templateFace++;
    }
    for (index = 0; index < MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_COUNT; index++) {
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vx = templateGrid->vertices[index].vx;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vy = templateGrid->vertices[index].vy;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vz = templateGrid->vertices[index].vz;
    }
    if (lowered == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = MIST_SHOOTING_GALLERY_TRAINING_BARRIER_LOWER_Y;
    }
    offset.vz = 0;
    // Preserve the eight-vertex offset pass: only the first four were restored.
    for (index = 0; index < MIST_SHOOTING_GALLERY_TRAINING_BARRIER_OFFSET_COUNT; index++) {
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vx += offset.vx;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vy += offset.vy;
        liveGrid->vertices[index + MIST_SHOOTING_GALLERY_TRAINING_BARRIER_VERTEX_OFFSET].vz += offset.vz;
    }
}

/// The jukebox's ten track lists: one per game mode, with list 4 standing in
/// before the first clear, and the second five used outside the debug attach
/// room.
static const JukeboxTrackLists _gJukeboxTrackLists = {
    {
        gJukeboxTracksAttach0,
        gJukeboxTracksAttach1,
        gJukeboxTracksAttach2,
        gJukeboxTracksAttach3,
        gJukeboxTracksAttach4,
        gJukeboxTracks0,
        gJukeboxTracks1,
        gJukeboxTracks2,
        gJukeboxTracks3,
        gJukeboxTracks4,
    },
};

/// "SELECT", followed by the non-zero padding the original toolchain left.
static const char D_mist_shooting_gallery_8017DB04[8] = "SELECT\0\xE1";

#include "../../shared/jukebox_row.inc.c"

/// Updates the gallery jukebox's track picker and asynchronous music change.
///
/// Borrows the task-owned UI object and singleton track list (three training
/// rows, four regular rows). The row callback supplies a pending sequence ID;
/// phase 1 waits for MIDI to stop, phase 2 waits for its file load, then starts it.
/// A cancel during loading hides the panel until playback starts. The initial
/// training sentinel keeps the panel open until a track has been selected.
static void _mistShootingGalleryJukeboxPanelTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_JUKEBOX_INITIALIZE          = 0,
        MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI           = 1,
        MIST_SHOOTING_GALLERY_JUKEBOX_TRACK_ID_LIMIT      = 0xF1,
        MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED = 0xFE,
        MIST_SHOOTING_GALLERY_JUKEBOX_IDLE                = 0xFF,
        MIST_SHOOTING_GALLERY_JUKEBOX_NO_ROW              = -1,
        MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS    = 10,
        MIST_SHOOTING_GALLERY_JUKEBOX_FILE_GROUP          = 4,
        MIST_SHOOTING_GALLERY_JUKEBOX_FILE_ID_HUNDREDS    = 1
    };
    u8        fileKey[4];
    u8        loadArgs[4];
    UiObject* object;
    UiList*   list;
    u8        sequenceId;
    s32       loadQueued;
    s32       phase;
    u8        sequenceStarted;

    object = task->spawnArg2.pointer;
    list   = &D_mist_shooting_gallery_80185338;

    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, D_mist_shooting_gallery_8017DB04);
    if (task->state == MIST_SHOOTING_GALLERY_JUKEBOX_INITIALIZE) {
        task->spawnArg1.value = MIST_SHOOTING_GALLERY_JUKEBOX_NO_ROW;
        if (attachmentIsTrainingMode() == 0) {
            list->itemCount = ARRAY_SIZE(gJukeboxTracks0);
        } else {
            list->itemCount = ARRAY_SIZE(gJukeboxTracksAttach0);
        }
        if (list->itemCount > MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS) {
            list->visibleRowCount.unsignedValue = MIST_SHOOTING_GALLERY_JUKEBOX_MAX_VISIBLE_ROWS;
        } else {
            list->visibleRowCount.unsignedValue = list->itemCount;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        uiFitPanelToList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        object->panel.bounds.unsignedRect.x = -((s16)object->panel.bounds.unsignedRect.w / 2);
        object->panel.bounds.unsignedRect.y = -((s16)object->panel.bounds.unsignedRect.h / 2);
        if (attachmentIsTrainingMode() == 0) {
            task->status = MIST_SHOOTING_GALLERY_JUKEBOX_IDLE;
        } else {
            task->status = MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED;
        }
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    sequenceId = task->status;
    if (sequenceId < MIST_SHOOTING_GALLERY_JUKEBOX_TRACK_ID_LIMIT) {
        // Stop the old sequence before loading its replacement; start after CD idle.
        phase = task->state;
        if (phase == MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI) {
            if (midiIsSequenceBusy(0) == 0) {
                fileKey[3]  = 0;
                fileKey[2]  = MIST_SHOOTING_GALLERY_JUKEBOX_FILE_GROUP;
                fileKey[0]  = sequenceId;
                loadArgs[0] = MIST_SHOOTING_GALLERY_JUKEBOX_FILE_ID_HUNDREDS;
                loadArgs[3] = 0;
                loadArgs[2] = 0;
                loadArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
                loadQueued = 1;
            } else {
                loadQueued = 0;
            }
            if (loadQueued == 1) {
                task->state += 1;
            }
        } else {
            if (cdCmdIsIdle() & 0xFFFF) {
                sndEvtRequestMidiStart(sequenceId, 0);
                sndEvtRequestMidiVolume(sequenceId, (u8)D_8007A396);
                sequenceStarted = 1;
                gStageRoomSong  = sequenceId;
            } else {
                sequenceStarted = 0;
            }
            if (sequenceStarted == 1) {
                task->state  = MIST_SHOOTING_GALLERY_JUKEBOX_WAIT_MIDI;
                task->status = MIST_SHOOTING_GALLERY_JUKEBOX_IDLE;
                if (attachmentIsTrainingMode() == 0) {
                    gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
                }
                if (object->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu | Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            if (task->status != MIST_SHOOTING_GALLERY_JUKEBOX_TRAINING_UNSELECTED) {
                if (task->status == MIST_SHOOTING_GALLERY_JUKEBOX_IDLE) {
                    object->result = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    uiStartPanelHiding(object, object->owner);
                    object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }
}

#include "../../shared/jukebox_host.inc.c"

s32 mistShootingGalleryOpenJukebox(s32 unused)
{
    displayQueueModeTask(&D_mist_shooting_gallery_80185378, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

/// Queues a 320x240 subtractive tile for the mode splash's current ramp tick.
///
/// Nonzero `fadeIn` complements the low countdown byte; zero uses it directly
/// for all RGB channels. Requires a live frame packet arena with room for a TILE
/// and DR_TPAGE at ordering-table entry 0. The current draw origin is retained,
/// including shake; packets live until GPU completion. Prepending the mode
/// after the tile makes subtractive blending and dithering active before drawing.
static inline void _mistShootingGalleryDrawSplashFade(const Task* task, s32 fadeIn)
{
    enum {
        MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS       = 320,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS      = 240,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_TEXTURE_DEPTH_4BIT = 0,
    };
    TILE*     tile;
    DR_TPAGE* drawMode;
    u8        intensity;

    tile           = gGpuPrimCursor;
    intensity      = fadeIn ? ~(u8)task->killCountdown : (u8)task->killCountdown;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = intensity;
    tile->g0 = intensity;
    tile->b0 = intensity;
    tile->x0 = -MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS / 2;
    tile->w  = MIST_SHOOTING_GALLERY_SPLASH_FADE_WIDTH_PIXELS;
    tile->h  = MIST_SHOOTING_GALLERY_SPLASH_FADE_HEIGHT_PIXELS;

    addPrim(gGpuCurrentOt, tile);
    // Insertion prepends, so the mode queued last runs before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(MIST_SHOOTING_GALLERY_SPLASH_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Displays the cleared run's mode splash with a fade in, hold and fade out.
///
/// The live mode must be 0..3, selecting stage-zero image file 41..44. Borrows
/// the serialized CD/display session and frame arena. Ramp steps are 8 per tick;
/// the hold ends after 151 ticks or Start. Completion blanks the display, clears
/// the complete image buffer and resumes the game loop after killing this task.
static void _mistShootingGalleryModeSplashTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_SPLASH_QUEUE_FILE               = 0,
        MIST_SHOOTING_GALLERY_SPLASH_WAIT_FILE                = 1,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_IN                  = 2,
        MIST_SHOOTING_GALLERY_SPLASH_HOLD                     = 3,
        MIST_SHOOTING_GALLERY_SPLASH_FADE_OUT                 = 4,
        MIST_SHOOTING_GALLERY_SPLASH_FINISH                   = 5,
        MIST_SHOOTING_GALLERY_SPLASH_REPLAY_FILE              = 41,
        MIST_SHOOTING_GALLERY_SPLASH_BOUNTY_FILE              = 42,
        MIST_SHOOTING_GALLERY_SPLASH_SCAVENGER_FILE           = 43,
        MIST_SHOOTING_GALLERY_SPLASH_NIGHTMARE_FILE           = 44,
        MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP           = 8,
        MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END            = 256,
        MIST_SHOOTING_GALLERY_SPLASH_DISPLAY_ENABLE_THRESHOLD = 17,
        MIST_SHOOTING_GALLERY_SPLASH_HOLD_TICKS               = 151
    };
    u8 fileKey[4];
    u8 loadArgs[4];

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_SPLASH_QUEUE_FILE:
            fileKey[3]  = 0;
            fileKey[2]  = 0;
            loadArgs[0] = 0;
            loadArgs[1] = 0;
            loadArgs[2] = 0;
            loadArgs[3] = 0;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 0:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_REPLAY_FILE;
                    break;
                case 1:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_BOUNTY_FILE;
                    break;
                case 2:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_SCAVENGER_FILE;
                    break;
                case 3:
                    fileKey[0] = MIST_SHOOTING_GALLERY_SPLASH_NIGHTMARE_FILE;
                    break;
            }
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadArgs);
            task->state++;
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_WAIT_FILE:
            if (cdCmdIsIdle() & 0xFFFF) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                task->killCountdown                     = 0;
                task->state++;
                return;
            }
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_FADE_IN: {
            // Keep display blank until the initial black overlay has started fading.
            _mistShootingGalleryDrawSplashFade(task, true);

            task->killCountdown += MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP;
            if (task->killCountdown >= MIST_SHOOTING_GALLERY_SPLASH_DISPLAY_ENABLE_THRESHOLD) {
                SetDispMask(1);
            }
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;
        }

        case MIST_SHOOTING_GALLERY_SPLASH_HOLD:
            task->killCountdown += 1;
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_HOLD_TICKS && padIsStartPressed() == 0) {
                return;
            }
            task->killCountdown = 0;
            task->state++;
            return;

        case MIST_SHOOTING_GALLERY_SPLASH_FADE_OUT: {
            _mistShootingGalleryDrawSplashFade(task, false);

            task->killCountdown += MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_STEP;
            if (task->killCountdown < MIST_SHOOTING_GALLERY_SPLASH_INTENSITY_END) {
                return;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(0);
            task->state++;
            return;
        }

        case MIST_SHOOTING_GALLERY_SPLASH_FINISH:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskKill(task);
            displayResumeGameLoop();
            return;

        default:
            return;
    }
}

/// Plays the gallery's post-clear movie and restores game graphics afterwards.
///
/// The current room must have a loaded view-100, sub-ID-zero movie slot. The
/// display/CD session must be serialized; this task reserves movie workspace,
/// waits for startup, accepts Start to cancel, then waits for CD idle and game
/// restoration. Completion clears the full image buffer and resumes the loop.
static void _mistShootingGalleryClearMovieTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_MOVIE_PREPARE    = 0,
        MIST_SHOOTING_GALLERY_MOVIE_QUEUE      = 1,
        MIST_SHOOTING_GALLERY_MOVIE_WAIT_READY = 2,
        MIST_SHOOTING_GALLERY_MOVIE_PLAY       = 3,
        MIST_SHOOTING_GALLERY_MOVIE_WAIT_STOP  = 4,
        MIST_SHOOTING_GALLERY_MOVIE_RESTORE    = 5,
        MIST_SHOOTING_GALLERY_CLEAR_MOVIE_VIEW = 100
    };
    u8          streamArgs[4];
    GameLoc     movieLocation;
    s16         movieSlot;
    CdCmdQueue* cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case MIST_SHOOTING_GALLERY_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_QUEUE:
            movieLocation          = gGameSession->location;
            movieLocation.loc.view = MIST_SHOOTING_GALLERY_CLEAR_MOVIE_VIEW;
            movieSlot              = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            streamArgs[0]          = movieSlot;
            // This opcode consumes only the slot byte; its other bytes are retained.
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_WAIT_READY:
            if (cdQueue->movieReady == 0) {
                break;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_PLAY:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                break;
            }
            if (padIsStartPressed() == 0) {
                break;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_WAIT_STOP:
            // Do not release the decoder workspace until playback/cancellation ends.
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            break;
        case MIST_SHOOTING_GALLERY_MOVIE_RESTORE:
            if ((streamPollGameRestore(0, 1) & 0xFFFF) == 0) {
                break;
            }
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            taskKill(task);
            displayResumeGameLoop();
            break;
    }
}

void func_mist_shooting_gallery_801810D8(Task* task)
{
    switch (task->state) {
        case 0:
            SetDispMask(0);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount == 0) {
                task->state = 2;
                return;
            }
            displaySpawnTaskFromTable(D_mist_shooting_gallery_80185384, 2, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            viewQueueCurrentCameraAndPackets();
        case 1:
            task->state = task->state + 1;
            return;
        case 2:
            displaySpawnTaskFromTable(D_mist_shooting_gallery_80185384, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            viewQueueCurrentCameraAndPackets();
            taskKill(task);
            return;
    }
}

void mistShootingGallerySelectRoomLights(s16 useAlternate)
{
    if (useAlternate == 0) {
        gMistShootingGalleryRoomLightingTable[0].lights = &gMistShootingGalleryDefaultRoomLights;
        return;
    }
    gMistShootingGalleryRoomLightingTable[0].lights = &D_mist_shooting_gallery_8018DF38;
}

void mistShootingGalleryDrawLightGlowsTask(Task* unused)
{
    enum {
        MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE = 0x200,
        MIST_SHOOTING_GALLERY_CAPSULE_COLOR        = 0x222,
        MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE    = 0x300,
        MIST_SHOOTING_GALLERY_DISC_COLOR           = 0x111
    };
    u8 mappedView;

    // View mapping selects the visible strips and point glows in world space.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2:
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[0], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[8], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[10], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 3:
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[4], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[6], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[14], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[16], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[18], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 7:
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[14], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[16], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[18], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[20], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 8:
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[24], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            break;
        case 9:
        case 18:
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[20], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[22], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[26], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            _glowDrawCapsule(&D_mist_shooting_gallery_80185550[28], MIST_SHOOTING_GALLERY_CAPSULE_RADIUS_SCALE, MIST_SHOOTING_GALLERY_CAPSULE_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[36], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[44], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 10:
            glowDrawDisc(&D_mist_shooting_gallery_80185550[37], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[43], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 11:
            glowDrawDisc(&D_mist_shooting_gallery_80185550[38], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[42], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 12:
            glowDrawDisc(&D_mist_shooting_gallery_80185550[40], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 13:
            glowDrawDisc(&D_mist_shooting_gallery_80185550[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
        case 14:
            glowDrawDisc(&D_mist_shooting_gallery_80185550[36], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[37], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[39], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[41], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[43], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            glowDrawDisc(&D_mist_shooting_gallery_80185550[44], MIST_SHOOTING_GALLERY_DISC_RADIUS_SCALE, MIST_SHOOTING_GALLERY_DISC_COLOR);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
