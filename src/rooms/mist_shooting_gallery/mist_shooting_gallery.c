#include "rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
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

#define D_mist_shooting_gallery_80185570 (D_mist_shooting_gallery_80185550 + 4)
#define D_mist_shooting_gallery_801855C0 (D_mist_shooting_gallery_80185550 + 14)
#define D_mist_shooting_gallery_801855F0 (D_mist_shooting_gallery_80185550 + 20)
#define D_mist_shooting_gallery_80185610 (D_mist_shooting_gallery_80185550 + 24)
#define D_mist_shooting_gallery_80185670 (D_mist_shooting_gallery_80185550 + 36)
#define D_mist_shooting_gallery_80185678 (D_mist_shooting_gallery_80185550 + 37)
#define D_mist_shooting_gallery_80185680 (D_mist_shooting_gallery_80185550 + 38)
#define D_mist_shooting_gallery_80185688 (D_mist_shooting_gallery_80185550 + 39)
#define D_mist_shooting_gallery_80185690 (D_mist_shooting_gallery_80185550 + 40)
#define D_mist_shooting_gallery_801856B0 (D_mist_shooting_gallery_80185550 + 44)

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
extern void func_actor_215100_8014AF0C(void);
extern void func_actor_215100_8014C5E0(s16, s16, s16);

extern s32        D_actor_215100_8014D038;
extern TaskDesc   D_actor_215100_8014E13C[];
extern EvsCommand D_actor_215100_80153274[];
extern EvsCommand D_actor_215100_80153D6C[];

/// Screen-fade "overlay owns the display" flag, first byte of the flag block
/// at 0x80071068. Declared as an array on purpose: GCC 2.8.1 exempts a
/// *fixed-address scalar* store from aliasing with a varying-address struct
/// load, so a plain `extern s8` here lets the scheduler hoist the following
/// `arg0->state` load above the store. Indexing an array makes the store a
/// struct reference and keeps the two in order.

/// The ten weapons the gallery's weapon picker offers, in row order. Rows whose
/// item is not unlocked yet (`func_800B7420` returns 0) are skipped, so
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
/// `jukeboxDrawRow`.
extern UiList D_mist_shooting_gallery_80185338;

/// The jukebox panel's descriptor; its update routine is the menu task
/// `func_mist_shooting_gallery_80180728`.
extern UiObjectDesc gJukeboxPanelDesc;

extern TaskDesc D_mist_shooting_gallery_80185378;

static void func_mist_shooting_gallery_801801E4(s32 arg0);

static const char D_mist_shooting_gallery_8017D65C[];

void func_mist_shooting_gallery_80180728(Task*);
void func_mist_shooting_gallery_80180B64(Task*);
void func_mist_shooting_gallery_80180F2C(Task*);
void func_mist_shooting_gallery_801810D8(Task*);

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
s32               func_mist_shooting_gallery_8017FEB0(Task*, s32, s32, s32);
s32               func_mist_shooting_gallery_8017FEB8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32               func_mist_shooting_gallery_80180000(Task*, s32, s32, s32);
s32               func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, const void* firstArg, s32 arg3);
void              func_mist_shooting_gallery_8017E234(Task*);
void              func_mist_shooting_gallery_8017E854(Task*);
void              func_mist_shooting_gallery_8017EAE0(Task*);
void              func_mist_shooting_gallery_8017EC58(Task*);
void              func_mist_shooting_gallery_8017F128(Task*);
void              func_mist_shooting_gallery_8017F6C8(Task*);
void              func_mist_shooting_gallery_8017F98C(UiList*, UiObject*);
void              func_mist_shooting_gallery_8017FAE8(Task*);
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
void              func_mist_shooting_gallery_8017E090(Task*);

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

UiObjectDesc D_mist_shooting_gallery_80184F70 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -8, 0, 144, 64 }, 32, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017E090, 0 };

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
    func_mist_shooting_gallery_8017F98C,
};

UiList D_mist_shooting_gallery_8018503C = { D_mist_shooting_gallery_80185038, 4, { .unsignedValue = 4 }, 1, 15, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc D_mist_shooting_gallery_80185060 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -96, 176, 64 }, 32, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017EAE0, 0 };

UiObjectDesc D_mist_shooting_gallery_8018507C[3] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { 32, -96, 112, 128 }, 28, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017EC58, 0 },
    { 3, { -144, 32, 288, 48 }, 24, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017FAE8, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -22, 288, 73 }, 20, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_8017F128, 0 },
};

TaskDesc D_mist_shooting_gallery_801850D0 = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_8017F6C8, { .value = 0 } };

TaskDesc D_mist_shooting_gallery_801850DC = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_8017FDD0, { .value = 0 } };

TaskMessageEntry D_mist_shooting_gallery_801850E8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mist_shooting_gallery_8017FEB8 },
    { 5105, func_mist_shooting_gallery_8017FEB0 },
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
    jukeboxDrawRow,
};

UiList D_mist_shooting_gallery_80185338 = { D_mist_shooting_gallery_80185334, 1, { .unsignedValue = 1 }, 0, 17, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

UiObjectDesc gJukeboxPanelDesc = { USER_INTERFACE_PANEL_TITLE_STYLE, { -112, -64, 224, 128 }, 48, 0, TASK_BODY_NONE, 192, func_mist_shooting_gallery_80180728, 0 };

TaskDesc D_mist_shooting_gallery_80185378 = { { { TASK_BODY_NONE, 192 } }, jukeboxHostTask, { .value = 0 } };

TaskDesc D_mist_shooting_gallery_80185384[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_801810D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_80180F2C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_80180B64, { .value = 0 } },
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

static s32  func_mist_shooting_gallery_8017FA38(s32 score);
static void func_mist_shooting_gallery_8017FC2C(Task* arg0);
static void func_mist_shooting_gallery_8017FD40(Task* task);

void func_mist_shooting_gallery_8017DCAC(s32 mode)
{
    InventoryItemRange* scan;
    s32                 row;
    s32                 col;

    scan = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 3; col++) {
            Gp_DebugAttachLevels[col + row * 3] = 0;
        }
    }
    Gp_DebugAttachLevels[0] = 1;

    switch (mode) {
        case 1:
        case 2:
            break;
        case 3:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_GiveItem(scan, 0x40, 1);
            break;
        case 4:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_SetItemSeenBit(5, 1);
            Gp_GiveItem(scan, 0x40, 1)->attachSlot = 2;
            Gp_GiveItem(scan, 5, 1)->attachSlot    = 3;
            Gp_GiveItem(scan, 5, 1)->attachSlot    = 4;
            Gp_DebugAttachLevels[0xA]              = 1;
            Gp_DebugAttachLevels[1]                = 1;
            break;
        case 5:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_SetItemSeenBit(1, 1);
            Gp_SetItemSeenBit(6, 1);
            Gp_GiveItem(scan, 0x40, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 6, 1);
            Gp_GiveItem(scan, 6, 1);
            Gp_DebugAttachLevels[0xA] = 1;
            Gp_DebugAttachLevels[1]   = 1;
            break;
    }
    Gp_FillHpMp();
    Gp_ApplyItemMap();
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
        if (func_800B7420(D_mist_shooting_gallery_80184F34[i]) != 0) {
            skip--;
            if (skip < 0) {
                item = D_mist_shooting_gallery_80184F34[i];
                break;
            }
        }
        i++;
    } while (i < 10);

    Gp_DrawItemLabel(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, item, arg0->colorRgb, 0);
    status = arg1->panel.control.word;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            Gp_SetPreviewItem(item, 0);
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
            Gp_ResetScanDefault();
            Gp_ClearScanItems(scan);
            Gp_GiveItem(scan, item, 1);
            Gp_GiveItem(scan, 0x6C, 1);
            Gp_EquipMod(0x6C);
            Gp_GiveItem(scan, ammo, 0x3E7)->attachSlot = selected;
            Gp_EquipRelatedItem(scan, item, ammo, -1);
            Gp_FillHpMp();
            arg1->result = USER_INTERFACE_RESULT_CONFIRM;
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
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

void func_mist_shooting_gallery_8017E090(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16*      weapon;
    s32       i;
    s32       count;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    list        = &D_mist_shooting_gallery_80184F4C;
    uiDrawPanelLabel(&obj->panel, D_mist_shooting_gallery_8017D5D8);
    if (task->state == 0) {
        count  = 0;
        i      = count;
        weapon = D_mist_shooting_gallery_80184F34;
        do {
            if (func_800B7420(*weapon) != 0) {
                Gp_SetItemSeenBit(*weapon, 1);
                count += 1;
            }
            i++;
            weapon++;
        } while (i < 10);

        list->itemCount = count;
        if ((u8)count >= 0xB) {
            list->visibleRowCount.unsignedValue = 0xA;
        } else {
            list->visibleRowCount.unsignedValue = count;
        }
        list->selectedItemIndex                   = 0;
        list->firstVisibleItemIndex.unsignedValue = 0;
        Ui_LayoutListPanel(list, &(obj)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        Ui_SetListScrollFlag(list, 1);
        obj->panel.bounds.unsignedRect.x = -((s16)obj->panel.bounds.unsignedRect.w / 2);
        obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
        task->state                     += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CANCEL || childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(childObj, childObj->owner);
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
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
        bonus = func_mist_shooting_gallery_80184470(total);
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
                    Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018501C, total, 1, 1, obj);
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
        bonus = func_mist_shooting_gallery_80184470(score);
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
    textDrawString(&req4, textItoaSigned(buf, func_mist_shooting_gallery_80184470(score)));

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
        Ui_LayoutListPanel(list, &(obj)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[0], 0, 0, 1, obj);
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[1], 0, 0, 1, obj);
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[2], 0, 0, 1, obj);
        task->state = task->state + 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0)) {
        sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}
void func_mist_shooting_gallery_8017EC58(Task* task)
{
    u8          buf[0x20];
    TextDrawReq req1;
    TextDrawReq req2;
    TextDrawReq req3;
    TextDrawReq req4;
    UiObject*   obj;
    s32         val;
    s32         q;
    s32         color;
    s32         rawExp;
    s32         rawBp;
    s32         xOff;
    s16         top;
    s32         y;

    obj = task->spawnArg2.pointer;
    uiDrawTitle(&(obj)->panel, "STATUS");
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(4));
        task->state = task->state + 1;
    }

    color = 0x606060;
    top   = obj->panel.contentTop.signedValue;
    y     = top + 0xF;
    val   = Gp_StatRows[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseHp.hpWord;
    xOff  = obj->panel.contentLeft.signedValue + 6;
    if (val < 100) {
        color = 0xD287F;
    }

    req1.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req1.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req1.otIndex    = obj->panel.otIndex.signedValue + 1;
    req1.colorRgb   = 0x606060;
    req1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req1.alignment  = TEXT_ALIGNMENT_LEFT;
    req1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req1, "HP");
    textDrawUiLine(obj, -xOff, y, textItoaSigned(buf, val), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    y     = top + 0x1E;
    val   = Gp_StatRows[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode].baseMp;
    color = 0x606060;
    if (val < 30) {
        color = 0xD287F;
    }

    req2.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req2.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req2.otIndex    = obj->panel.otIndex.signedValue + 1;
    req2.colorRgb   = 0x606060;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_LEFT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req2, "MP");
    textDrawUiLine(obj, -xOff, y, textItoaSigned(buf, val), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    y      = top + 0x2D;
    rawExp = D_mist_shooting_gallery_8018E0BC;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case 3:
            val = 0;
            break;
        case 2:
            q = rawExp / 100;
            goto clamp_exp;
        case 1:
            q = rawExp / 20;
            goto clamp_exp;
        default:
            q = rawExp / 10;
        clamp_exp:
            if (q > 999999) {
                q = 999999;
            }
            val = q;
            break;
    }

    req3.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req3.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req3.otIndex    = obj->panel.otIndex.signedValue + 1;
    req3.colorRgb   = 0x606060;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_LEFT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req3, "EXP");
    textDrawUiLine(obj, -xOff, y, textItoaSigned(buf, val), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);

    y    += 0xF;
    rawBp = D_mist_shooting_gallery_8018E0C0;
    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case 3:
            val = 0;
            break;
        case 2:
            q = rawBp / 100;
            goto clamp_bp;
        case 1:
            q = rawBp / 20;
            goto clamp_bp;
        default:
            q = rawBp / 10;
        clamp_bp:
            if (q > 999999) {
                q = 999999;
            }
            val = q;
            break;
    }

    req4.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req4.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 8) + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = 0x606060;
    req4.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req4.alignment  = TEXT_ALIGNMENT_LEFT;
    req4.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req4, "BP");
    textDrawUiLine(obj, -xOff, y, textItoaSigned(buf, val), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
}
static const _MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D708 = { { D_mist_shooting_gallery_80184DD4, D_mist_shooting_gallery_80184E24, D_mist_shooting_gallery_80184E70, D_mist_shooting_gallery_80184EC4 } };

void func_mist_shooting_gallery_8017F128(Task* task)
{
    UiObject* obj = task->spawnArg2.pointer;
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
    TextDrawReq                 label0;
    TextDrawReq                 value0;
    TextDrawReq                 label1;
    TextDrawReq                 value1;
    TextDrawReq                 label2;
    TextDrawReq                 value2;
    TextDrawReq                 label3;
    TextDrawReq                 value3;
    _MistShootingGalleryRating* rating;
    s32                         col;
    s32                         row;
    s32                         x;
    s32                         y;

    uiDrawTitle(&(obj)->panel, D_mist_shooting_gallery_8017D820);

    col               = obj->panel.contentLeft.signedValue;
    obj->result       = USER_INTERFACE_RESULT_NONE;
    x                 = col + 0xB;
    row               = obj->panel.contentTop.signedValue;
    label0.x          = obj->panel.contentOriginX.unsignedValue + x;
    y                 = row + 0xB;
    label0.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 6) + y;
    label0.otIndex    = obj->panel.otIndex.signedValue + 1;
    rating            = &missionLevels[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];
    label0.colorRgb   = 0x606060;
    label0.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    label0.alignment  = TEXT_ALIGNMENT_LEFT;
    label0.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&label0, D_mist_shooting_gallery_8017D828);

    value0.x          = obj->panel.contentOriginX.unsignedValue + 0x41;
    value0.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 3) + y;
    value0.otIndex    = obj->panel.otIndex.signedValue + 1;
    value0.colorRgb   = 0x606060;
    value0.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    value0.alignment  = TEXT_ALIGNMENT_RIGHT;
    value0.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&value0, rating->name);
    textDrawUiLine(obj, 0x46, y, (const u8*)gaugeByLevel[rating->level], 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
    uiDrawHorizontalSeparator(&(obj)->panel, col + 6, -x + 5, row + 0xD);

    y                 = row + 0x1E;
    label1.x          = obj->panel.contentOriginX.unsignedValue + x;
    label1.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 6) + y;
    label1.otIndex    = obj->panel.otIndex.signedValue + 1;
    rating            = &conditions[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];
    label1.colorRgb   = 0x606060;
    label1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    label1.alignment  = TEXT_ALIGNMENT_LEFT;
    label1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&label1, D_mist_shooting_gallery_8017D838);

    value1.x          = obj->panel.contentOriginX.unsignedValue + 0x41;
    value1.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 3) + y;
    value1.otIndex    = obj->panel.otIndex.signedValue + 1;
    value1.colorRgb   = 0x606060;
    value1.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    value1.alignment  = TEXT_ALIGNMENT_RIGHT;
    value1.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&value1, rating->name);
    textDrawUiLine(obj, 0x46, y, (const u8*)gaugeByLevel[rating->level], 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);

    y                 = row + 0x2D;
    label2.x          = obj->panel.contentOriginX.unsignedValue + x;
    label2.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 6) + y;
    label2.otIndex    = obj->panel.otIndex.signedValue + 1;
    rating            = &enemyLevels[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];
    label2.colorRgb   = 0x606060;
    label2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    label2.alignment  = TEXT_ALIGNMENT_LEFT;
    label2.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&label2, D_mist_shooting_gallery_8017D844);

    value2.x          = obj->panel.contentOriginX.unsignedValue + 0x41;
    value2.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 3) + y;
    value2.otIndex    = obj->panel.otIndex.signedValue + 1;
    value2.colorRgb   = 0x606060;
    value2.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    value2.alignment  = TEXT_ALIGNMENT_RIGHT;
    value2.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&value2, rating->name);
    textDrawUiLine(obj, 0x46, y, (const u8*)gaugeByLevel[rating->level], 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);

    y                 = row + 0x3C;
    label3.x          = obj->panel.contentOriginX.unsignedValue + x;
    label3.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 6) + y;
    label3.otIndex    = obj->panel.otIndex.signedValue + 1;
    rating            = &supplyLevels[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode];
    label3.colorRgb   = 0x606060;
    label3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    label3.alignment  = TEXT_ALIGNMENT_LEFT;
    label3.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&label3, D_mist_shooting_gallery_8017D850);

    value3.x          = obj->panel.contentOriginX.unsignedValue + 0x41;
    value3.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 3) + y;
    value3.otIndex    = obj->panel.otIndex.signedValue + 1;
    value3.colorRgb   = 0x606060;
    value3.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    value3.alignment  = TEXT_ALIGNMENT_RIGHT;
    value3.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&value3, rating->name);
    textDrawUiLine(obj, 0x46, y, (const u8*)gaugeByLevel[rating->level], 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_LEFT);
}
/// Task handler for the gallery's closing sequence. State 0 spawns the results
/// panel and stashes `gPlayerStatus.exp` / `gPlayerStatus.bp` in
/// `D_mist_shooting_gallery_8018E0BC` / `_8018E0C0`.
/// State 1 waits for the panel to confirm (`result == USER_INTERFACE_RESULT_CONFIRM`), then writes both
/// totals back scaled down by the bonus mode - the same divisor table as
/// `func_mist_shooting_gallery_8017FA38`, clamped to 999999. Once the kill
/// countdown runs out the task exits and the stage is flagged as ended.
void func_mist_shooting_gallery_8017F6C8(Task* task)
{
    UiObject*     obj;
    PlayerStatus* cfg = &gPlayerStatus;
    s32           savedBp;
    s32           savedExp;
    s32           bp;
    s32           exp;

    if (task->state == 0) {
        obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_80185060, 0, 1, 1, NULL);
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
            Gp_RecalcMaxHp();
            Gp_RecalcMaxMp();

            savedBp = D_mist_shooting_gallery_8018E0BC;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 3:
                    bp = 0;
                    goto store_bp;
                case 2:
                    bp = savedBp / 100;
                    break;
                case 1:
                    bp = savedBp / 20;
                    break;
                default:
                    bp = savedBp / 10;
                    break;
            }
            if (bp > 999999) {
                bp = 999999;
            }
        store_bp:
            cfg->exp = bp;

            savedExp = D_mist_shooting_gallery_8018E0C0;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 3:
                    exp = 0;
                    goto store_exp;
                case 2:
                    exp = savedExp / 100;
                    break;
                case 1:
                    exp = savedExp / 20;
                    break;
                default:
                    exp = savedExp / 10;
                    break;
            }
            if (exp > 999999) {
                exp = 999999;
            }
        store_exp:
            cfg->bp = exp;
            Gp_FillHpMp();
            task->state = task->state + 1;
        }
    } else {
        task->killCountdown = task->killCountdown - 1;
        if (task->killCountdown < 0) {
            taskCallExit(task);
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            Wip_UiHolder = NULL;
            Stage_SetEndingFlag();
        }
    }
}
s32 func_mist_shooting_gallery_8017F95C(s32 unused)
{
    Display_InitModeObj(&D_mist_shooting_gallery_80184F8C, 0x44, 0, 0);
    return 1;
}

void func_mist_shooting_gallery_8017F98C(UiList* arg0, UiObject* arg1)
{
    _MistShootingGalleryModeTexts modeNames;
    s32                           one;

    modeNames = D_mist_shooting_gallery_8017D6D8;
    one       = 1;
    // The list has one row per run mode, so the row index is the mode.
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue - 1, (const u8*)modeNames.byMode[arg0->currentItemIndex], arg0->colorRgb, one, TEXT_ALIGNMENT_LEFT);
    if (arg0->rowInputEnabled == one) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode = (u8)arg0->currentItemIndex;
    }
}
static s32 func_mist_shooting_gallery_8017FA38(s32 score)
{
    s32 value;

    switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
        case 3:
            return 0;
        case 2:
            value = score / 100;
            break;
        case 1:
            value = score / 20;
            break;
        default:
            value = score / 10;
            break;
    }
    if (value > 999999) {
        value = 999999;
    }
    return value;
}
void func_mist_shooting_gallery_8017FAE8(Task* task)
{
    UiObject*                     obj              = task->spawnArg2.pointer;
    _MistShootingGalleryModeTexts modeDescriptions = D_mist_shooting_gallery_8017D708;

    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        uiSetPanelContentSize(&(obj)->panel, 0, uiGetTextRowsHeight(3) + 1);
        obj->panel.bounds.unsignedRect.y = 0x68 - obj->panel.bounds.unsignedRect.h;
        task->state                      = task->state + 1;
    }
    textDrawUiLines(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, (const u8*)modeDescriptions.byMode[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode], 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}
void func_mist_shooting_gallery_8017FBD8(void)
{
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) && (gGameSession->location.loc.warp == 7)) {
        Display_InitModeObj(&D_mist_shooting_gallery_801850D0, 0, 0, 0);
    }
}
static void func_mist_shooting_gallery_8017FC2C(Task* arg0)
{
    s32 var_a0;

    arg0->msgTable = D_mist_shooting_gallery_801850E8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    func_actor_215100_8014C5E0(0x340, 0, 2);
    if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
        Gp_MsgSlot4Chain(1, 0);
        var_a0 = 1;
    } else {
        var_a0 = 0;
    }
    func_mist_shooting_gallery_801801E4(var_a0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 7) {
        taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 0, 0, 0);
    } else if (gGameSession->location.loc.warp == 7) {
        taskSpawnFromTable(D_actor_215100_8014E13C, 0, 0, 0);
    }
    if ((gGameSession->location.loc.warp == 6) && (gameFlagGetNibble(GAME_FLAG_0ED) != 0)) {
        Gp_RunCapCmd1(0x16);
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
            Gp_MsgSlot4Chain(1, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_0ED) == 0) {
            Gp_MsgSlot4Chain(1, 1);
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
                Gp_LoadCapFile(MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_HIGH_COMMANDS;
            } else {
                Gp_LoadCapFile(MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
                texturePageX = MIST_SHOOTING_GALLERY_CAP_TEXTURE_X_LOW_COMMANDS;
            }
            func_800E6D4C(texturePageX, 0);
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            goto block_inc;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
        block_inc:
            task->state += 1;
            return;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
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

/// The room's handler for message 0x13F1: accepts it and does nothing.
s32 func_mist_shooting_gallery_8017FEB0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
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
                Gp_ResetInventory();
            }
            if (dst->warp == 5) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 1;
                gPlayerStatus.resourceVariant                       = 3;
                gGameSession->hideHud                               = 1;
                Gp_ClearInventory();
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
                    Gp_MsgPlayerWeapon(0);
                    taskSpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, MIST_SHOOTING_GALLERY_CAP_FILE_HIGH_COMMANDS);
                }
            }
        } else {
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, MIST_SHOOTING_GALLERY_CAP_FILE_LOW_COMMANDS);
        }
    }
    return 0;
}

s32 func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if ((request->actionId == 1) && (D_actor_215100_8014D038 == 0)) {
        Gp_MsgPlayerWeapon(0);
        taskSpawnFromTable(D_actor_215100_8014E13C, 1, 1, 0);
        D_80114D08 = 0xA;
    }
    if ((request->actionId == 2) && (gameFlagGetNibble(GAME_FLAG_0ED) == 0)) {
        func_actor_215100_8014AF0C();
    }
    if (request->actionId == 3) {
        func_actor_215100_8014AB6C();
    }
    if ((request->actionId == 4) && (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN) == 0)) {
        func_800E3FAC(0xA2, 0x3B);
        gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_ACTION_4_SEEN, 1);
        func_800E8634(D_actor_215100_80153274, 0, D_actor_215100_80153D6C);
    }
    return 0;
}

/// The room task's three-state table, run from a stack copy by
/// `func_mist_shooting_gallery_8018018C`: the entry tick
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

/// The room task: copies the three-state table
/// `D_mist_shooting_gallery_8017D860` onto the stack and runs the entry for the
/// task's current state - the entry tick `func_mist_shooting_gallery_8017FC2C`,
/// the per-frame state `func_mist_shooting_gallery_8017FD40`, then `taskKill`.
void func_mist_shooting_gallery_8018018C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_shooting_gallery_8017D860;
    sp.funcs[task->state](task);
}

static void func_mist_shooting_gallery_801801E4(s32 arg0)
{
    WorldCollisionGrid* dst = &D_mist_shooting_gallery_80189968;
    WorldCollisionGrid* src = &D_mist_shooting_gallery_80185198;
    SVECTOR             ofs;
    s32                 i;

    for (i = 0; i < 3; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }
    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }
    if (arg0 == 0) {
        ofs.vx = 0;
        ofs.vy = 0;
    } else {
        ofs.vx = 0;
        ofs.vy = 0xBB8;
    }
    ofs.vz = 0;
    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx += ofs.vx;
        dst->vertices[i].vy += ofs.vy;
        dst->vertices[i].vz += ofs.vz;
    }
}

void func_mist_shooting_gallery_80180390(s32 arg0)
{
    WorldCollisionGrid*     dst        = &D_mist_shooting_gallery_80189968;
    WorldCollisionGrid*     src        = &D_mist_shooting_gallery_801851F8;
    WorldCollisionGridFace* destFace   = &D_mist_shooting_gallery_80189968.faces[3];
    WorldCollisionGridFace* sourceFace = D_mist_shooting_gallery_801851F8.faces;
    SVECTOR                 ofs;
    s32                     i;
    s32                     j;

    for (i = 0; i < 1; i++) {
        dst->normals[i + 3].vx = src->normals[i].vx;
        dst->normals[i + 3].vy = src->normals[i].vy;
        dst->normals[i + 3].vz = src->normals[i].vz;
        for (j = 0; j < ARRAY_SIZE(destFace->vertexIndices); j++) {
            destFace->vertexIndices[j] = sourceFace->vertexIndices[j] + 8;
        }
        destFace->normalIndex  = sourceFace->normalIndex + 3;
        destFace->surfaceClass = sourceFace->surfaceClass;
        destFace++;
        sourceFace++;
    }
    for (i = 0; i < 4; i++) {
        dst->vertices[i + 8].vx = src->vertices[i].vx;
        dst->vertices[i + 8].vy = src->vertices[i].vy;
        dst->vertices[i + 8].vz = src->vertices[i].vz;
    }
    if (arg0 == 0) {
        ofs.vx = 0;
        ofs.vy = 0;
    } else {
        ofs.vx = 0;
        ofs.vy = 0xFA0;
    }
    ofs.vz = 0;
    for (i = 0; i < 8; i++) {
        dst->vertices[i + 8].vx += ofs.vx;
        dst->vertices[i + 8].vy += ofs.vy;
        dst->vertices[i + 8].vz += ofs.vz;
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

/// The jukebox menu task, the update routine of the panel
/// `gJukeboxPanelDesc` builds. Draws the title and, on its first
/// tick, lays out the track list (four rows, three in the debug attach room).
/// While a chosen track is pending it waits for the MIDI player to go idle,
/// queues the track's CD load, then starts it once the CD is idle and records
/// it as the current track. The menu or cancel button plays the back sound and
/// closes the panel.
void func_mist_shooting_gallery_80180728(Task* task)
{
    u8        param1[8];
    u8        param2[8];
    UiObject* obj;
    UiList*   menu;
    u8        flags;
    s32       sent;
    s32       state;
    u8        ready;

    obj  = task->spawnArg2.pointer;
    menu = &D_mist_shooting_gallery_80185338;

    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, D_mist_shooting_gallery_8017DB04);
    if (task->state == 0) {
        task->spawnArg1.value = -1;
        if (Gp_IsDebugAttachRoom() == 0) {
            menu->itemCount = 4;
        } else {
            menu->itemCount = 3;
        }
        if (menu->itemCount >= 0xB) {
            menu->visibleRowCount.unsignedValue = 0xA;
        } else {
            menu->visibleRowCount.unsignedValue = menu->itemCount;
        }
        menu->selectedItemIndex                   = 0;
        menu->firstVisibleItemIndex.unsignedValue = 0;
        Ui_LayoutListPanel(menu, &(obj)->panel);
        menu->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        Ui_SetListScrollFlag(menu, 1);
        obj->panel.bounds.unsignedRect.x = -((s16)obj->panel.bounds.unsignedRect.w / 2);
        obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
        if (Gp_IsDebugAttachRoom() == 0) {
            task->status = 0xFF;
        } else {
            task->status = 0xFE;
        }
        task->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    flags = task->status;
    if (flags < 0xF1) {
        state = task->state;
        if (state == 1) {
            if (Midi_IsBusy(0) == 0) {
                param1[3] = 0;
                param1[2] = 4;
                param1[0] = flags;
                param2[0] = state;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
                sent = 1;
            } else {
                sent = 0;
            }
            if (sent == 1) {
                task->state += 1;
            }
        } else {
            if (CdCmd_IsIdle() & 0xFFFF) {
                SndEvt_EnqueueType1(flags, 0);
                sndEvtRequestMidiVolume(flags, (u8)D_8007A396);
                ready          = 1;
                gStageRoomSong = flags;
            } else {
                ready = 0;
            }
            if (ready == 1) {
                task->state  = 1;
                task->status = 0xFF;
                if (Gp_IsDebugAttachRoom() == 0) {
                    gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
                }
                if (obj->panel.control.word != USER_INTERFACE_PANEL_ACTIVE) {
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                }
            }
        }
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskMenu | Pad_MaskCancel) != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            if (task->status != 0xFE) {
                if (task->status == 0xFF) {
                    obj->result = USER_INTERFACE_RESULT_CONFIRM;
                } else {
                    uiStartPanelHiding(obj, obj->owner);
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                }
            }
        }
    }
}

#include "../../shared/jukebox_host.inc.c"

s32 func_mist_shooting_gallery_80180B34(s32 unused)
{
    Display_InitModeObj(&D_mist_shooting_gallery_80185378, 0, 0, 0);
    return 1;
}

void func_mist_shooting_gallery_80180B64(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    switch (arg0->state) {
        case 0:
            param1[3] = 0;
            param1[2] = 0;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
                case 0:
                    param1[0] = 0x29;
                    break;
                case 1:
                    param1[0] = 0x2A;
                    break;
                case 2:
                    param1[0] = 0x2B;
                    break;
                case 3:
                    param1[0] = 0x2C;
                    break;
            }
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            arg0->state++;
            return;

        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                arg0->killCountdown                     = 0;
                arg0->state++;
                return;
            }
            return;

        case 2: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = gGpuPrimCursor;
            color          = ~(u8)arg0->killCountdown;
            gGpuPrimCursor = p + 1;
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(gGpuCurrentOt, p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown >= 0x11) {
                SetDispMask(1);
            }
            if (arg0->killCountdown < 0x100) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;
        }

        case 3:
            arg0->killCountdown += 1;
            if (arg0->killCountdown < 0x97 && Pad_CheckFlag800() == 0) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;

        case 4: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = gGpuPrimCursor;
            color          = (u8)arg0->killCountdown;
            gGpuPrimCursor = p + 1;
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(gGpuCurrentOt, p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown < 0x100) {
                return;
            }
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(0);
            arg0->state++;
            return;
        }

        case 5:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskKill(arg0);
            displayResumeGameLoop();
            return;

        default:
            return;
    }
}

void func_mist_shooting_gallery_80180F2C(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->location;
    key.loc.view = 0x64;
    slot         = streamFindMovieSlot(&key.loc, 0, 0);
    slotParam[0] = slot;
    cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    taskKill(task);
    displayResumeGameLoop();
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
            Display_SpawnWithOt(D_mist_shooting_gallery_80185384, 2, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_FULL;
            Gp_SpawnViewTasks();
        case 1:
            task->state = task->state + 1;
            return;
        case 2:
            Display_SpawnWithOt(D_mist_shooting_gallery_80185384, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            Gp_SpawnViewTasks();
            taskKill(task);
            return;
    }
}

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_mist_shooting_gallery_801811C0(s16 arg0)
{
    if (arg0 == 0) {
        gMistShootingGalleryRoomLightingTable[0].lights = &gMistShootingGalleryDefaultRoomLights;
        return;
    }
    gMistShootingGalleryRoomLightingTable[0].lights = &D_mist_shooting_gallery_8018DF38;
}

void func_mist_shooting_gallery_801811EC(Task* unused)
{
    u8 view;

    view = viewGetMappedIndex();
    switch (view) {
        case 2:
            glowDrawCapsule(&D_mist_shooting_gallery_80185550[0], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185550[8], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185550[10], 0x200, 0x222);
            break;
        case 3:
            glowDrawCapsule(&D_mist_shooting_gallery_80185570[0], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185570[2], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185570[10], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185570[12], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_80185570[14], 0x200, 0x222);
            break;
        case 7:
            glowDrawCapsule(&D_mist_shooting_gallery_801855C0[0], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855C0[2], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855C0[4], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855C0[6], 0x200, 0x222);
            break;
        case 8:
            glowDrawCapsule(&D_mist_shooting_gallery_80185610[0], 0x200, 0x222);
            break;
        case 9:
        case 18:
            glowDrawCapsule(&D_mist_shooting_gallery_801855F0[0], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855F0[2], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855F0[6], 0x200, 0x222);
            glowDrawCapsule(&D_mist_shooting_gallery_801855F0[8], 0x200, 0x222);
            glowDrawDisc(&D_mist_shooting_gallery_801855F0[16], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
        case 10:
            glowDrawDisc(&D_mist_shooting_gallery_80185678[0], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185678[2], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185678[4], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185678[6], 0x300, 0x111);
            break;
        case 11:
            glowDrawDisc(&D_mist_shooting_gallery_80185680[0], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185680[1], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185680[3], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185680[4], 0x300, 0x111);
            break;
        case 12:
            glowDrawDisc(&D_mist_shooting_gallery_80185690[0], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185690[1], 0x300, 0x111);
            break;
        case 13:
            glowDrawDisc(&D_mist_shooting_gallery_80185688[0], 0x300, 0x111);
            break;
        case 14:
            glowDrawDisc(&D_mist_shooting_gallery_80185670[0], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185670[1], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185670[3], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185670[5], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_80185670[7], 0x300, 0x111);
            glowDrawDisc(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"
