#include "rooms/shelter_r47.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "shelter_r47_private.h"

#include "actors/task_tables.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
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
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/action_prompt.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_143400_801350BC[];
extern EvsCommand D_actor_143400_801359D4[];
extern EvsCommand D_actor_443500_8014152C[];
extern EvsCommand D_actor_443500_80141C1C[];
extern EvsCommand D_actor_443500_80141D9C[];
extern EvsCommand D_actor_443500_80142A74[];
extern EvsCommand D_actor_443500_80142C24[];
extern EvsCommand D_actor_443500_801432FC[];
extern EvsCommand D_actor_443500_80143494[];

/// `clutX` value that ends a sprite's piece list. That record is not drawn.
enum { SHELTER_R47_SPRITE_PART_END = 0xFFFF };

/// Texture page every piece is drawn on: 4-bit texels at VRAM (832, 0).
enum { SHELTER_R47_SPRITE_PART_TPAGE = 0xD };

/// One textured quad of a composite sprite on this room's selection screen.
///
/// A sprite is a list of these records. The list ends at the record whose
/// `clutX` is `SHELTER_R47_SPRITE_PART_END`; that record is not drawn, and its
/// other fields are unread. Each earlier record is emitted as a raw-textured
/// flat quad on `SHELTER_R47_SPRITE_PART_TPAGE` into ordering-table slot 10.
/// The quad's screen rectangle is the caller's origin plus `x` and `y`, sized
/// `w` by `h`. The same `w` and `h` are the texture span from (`u`, `v`).
/// The palette is `getClut(clutX, clutY)`.
typedef struct {
    u16 clutX; // Palette X in VRAM pixels, aligned to 16. SHELTER_R47_SPRITE_PART_END ends the list.
    u16 clutY; // Palette Y in VRAM pixels.
    s16 x;     // X offset from the sprite origin, in pixels.
    s16 y;     // Y offset from the sprite origin, in pixels.
    u8  u;     // Left column of the texture rectangle, in texels.
    u8  v;     // Top row of the texture rectangle, in texels.
    u8  w;     // Width of the quad in pixels and of the texture rectangle in texels.
    u8  h;     // Height of the quad in pixels and of the texture rectangle in texels.
} _ShelterR47SpritePart;
STATIC_ASSERT_SIZEOF(_ShelterR47SpritePart, 0xC);

static TaskDesc             gRoomCutsceneTaskDescs[3];
static TaskMessageEntry     D_shelter_r47_80186F2C[6];
static AnimationPlayRequest D_shelter_r47_80186F5C;
static TaskDesc             D_shelter_r47_80186F70[3];
static TaskDesc             D_shelter_r47_80186F94[2];
static ActionPromptHotspot  D_shelter_r47_80186FB4[9];
static TaskDesc             D_shelter_r47_80187020;
/// Piece lists of the sprites `shelterR47ConsoleDrawSprite` draws, by sprite id.
static _ShelterR47SpritePart* D_shelter_r47_8018729C[];
static TaskDesc               D_shelter_r47_801872F0;
static s32                    _shelterR47HandleDirectionAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32                    _shelterR47HandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedMode);
static void                   _shelterR47AmbienceTask(Task* task);
static s32                    _shelterR47RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32                    _shelterR47ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                    _shelterR47HandleSoundCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static void                   _shelterR47PlayCapCommandTask(Task* task);
static void                   _shelterR47RestoreActorsAfterTerminalTask(Task* task);
static void                   _shelterR47PlayEntryScene8Task(Task* task);
static void                   _shelterR47PlayEntryScene7Task(Task* task);

/// Phases and loaded-resource choices of this room's CAP playback tasks.
enum {
    SHELTER_R47_CAP_START               = 0,
    SHELTER_R47_CAP_WAIT                = 1,
    SHELTER_R47_CAP_RELEASE             = 2,
    SHELTER_R47_CAP_MAIN_RESOURCE       = 1,
    SHELTER_R47_CAP_ALTERNATE_RESOURCE  = 2,
    SHELTER_R47_CAP_MAIN_TEXTURE_X      = 576,
    SHELTER_R47_CAP_ALTERNATE_TEXTURE_X = 320,
    SHELTER_R47_CAP_TEXTURE_Y           = 256,
};

/// Console wipe: byte-scale colours stepped each frame, and a 32-wedge disc
/// with 128 angle units per edge (4096 per turn), radius 256 screen pixels.
enum {
    SHELTER_R47_CONSOLE_WIPE_STEP        = 32,
    SHELTER_R47_CONSOLE_WIPE_CLEAR_NEXT  = 160,
    SHELTER_R47_CONSOLE_WIPE_FILL_NEXT   = 96,
    SHELTER_R47_CONSOLE_WIPE_MAX         = 255,
    SHELTER_R47_CONSOLE_WIPE_SEGMENTS    = 32,
    SHELTER_R47_CONSOLE_WIPE_ANGLE_SHIFT = 7,
    SHELTER_R47_CONSOLE_WIPE_TRIG_SHIFT  = 4,
    SHELTER_R47_CONSOLE_WIPE_OT          = 11,
};

#define TELEPHONE_TITLE_BYTES "Telephone\0\xDC" \
                              "2"
#include "../../shared/telephone.h"

static void _shelterR47InitializeRoomTask(Task* task);
static void _shelterR47AdvanceTerminalTour(Task* unusedTask);

#include "../../shared/telephone_data.inc.c"

static TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static TaskMessageEntry D_shelter_r47_80186F2C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterR47ResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterR47RefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterR47HandleDirectionAction },
    { ROOM_MESSAGE_COMMAND, _shelterR47HandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _shelterR47HandleSoundCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPlayRequest D_shelter_r47_80186F5C = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

static TaskDesc D_shelter_r47_80186F70[3] = {
    { { { TASK_BODY_NONE, 192 } }, _shelterR47RestoreActorsAfterTerminalTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterR47AmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterR47PlayCapCommandTask, { .value = 0 } },
};

static TaskDesc D_shelter_r47_80186F94[2] = {
    { { { TASK_BODY_NONE, 192 } }, _shelterR47PlayEntryScene8Task, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterR47PlayEntryScene7Task, { .value = 0 } },
};

u8 D_shelter_r47_80186FAC[5] = {
    16,
    18,
    17,
    20,
    19,
};

static ActionPromptHotspot D_shelter_r47_80186FB4[9] = {
    { 128, -104, 20, 20, 1025, 0, 0 },
    { -152, -104, 68, 20, 769, 0, 0 },
    { -152, 72, 72, 16, 257, 1, 0 },
    { 124, -83, 26, 16, 0, 1, 0 },
    { 124, -67, 26, 16, 1, 1, 0 },
    { 124, -51, 26, 16, 2, 1, 0 },
    { 124, -35, 26, 16, 3, 1, 0 },
    { 124, -19, 26, 16, 4, 1, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

static TaskDesc D_shelter_r47_80187020 = { { { TASK_BODY_NONE, 192 } }, shelterR47ConsoleTask, { .value = 0 } };

static _ShelterR47SpritePart D_shelter_r47_8018702C[2] = {
    { 32, 255, 0, 0, 120, 160, 72, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187044[2] = {
    { 96, 255, 0, 0, 0, 184, 72, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_8018705C[2] = {
    { 128, 255, 0, 0, 0, 184, 72, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187074[3] = {
    { 0, 255, 0, 0, 0, 0, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187098[3] = {
    { 0, 255, 0, 0, 0, 16, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_801870BC[3] = {
    { 0, 255, 0, 0, 0, 32, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_801870E0[3] = {
    { 0, 255, 0, 0, 0, 48, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187104[3] = {
    { 0, 255, 0, 0, 0, 64, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187128[3] = {
    { 0, 255, 0, 0, 0, 80, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_8018714C[3] = {
    { 0, 255, 0, 0, 0, 96, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187170[3] = {
    { 0, 255, 0, 0, 0, 112, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187194[3] = {
    { 0, 255, 0, 0, 0, 128, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_801871B8[3] = {
    { 0, 255, 0, 0, 0, 144, 255, 16 },
    { 48, 255, 255, 0, 72, 184, 48, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_801871DC[2] = {
    { 16, 255, 0, 0, 0, 160, 24, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_801871F4[2] = {
    { 16, 255, 0, 0, 24, 160, 24, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_8018720C[2] = {
    { 16, 255, 0, 0, 48, 160, 24, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187224[2] = {
    { 16, 255, 0, 0, 72, 160, 24, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_8018723C[2] = {
    { 16, 255, 0, 0, 96, 160, 24, 24 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187254[2] = {
    { 80, 255, 0, 0, 120, 184, 40, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_8018726C[2] = {
    { 112, 255, 0, 0, 120, 184, 40, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart D_shelter_r47_80187284[2] = {
    { 64, 255, 0, 0, 160, 184, 16, 16 },
    { SHELTER_R47_SPRITE_PART_END, 0, 0, 0, 0, 0, 0, 0 },
};

static _ShelterR47SpritePart* D_shelter_r47_8018729C[21] = {
    D_shelter_r47_8018702C,
    D_shelter_r47_80187044,
    D_shelter_r47_8018705C,
    D_shelter_r47_80187074,
    D_shelter_r47_80187098,
    D_shelter_r47_801870BC,
    D_shelter_r47_801870E0,
    D_shelter_r47_80187128,
    D_shelter_r47_80187104,
    D_shelter_r47_8018714C,
    D_shelter_r47_80187170,
    D_shelter_r47_80187194,
    D_shelter_r47_801871B8,
    D_shelter_r47_801871DC,
    D_shelter_r47_801871F4,
    D_shelter_r47_8018720C,
    D_shelter_r47_80187224,
    D_shelter_r47_8018723C,
    D_shelter_r47_80187254,
    D_shelter_r47_8018726C,
    D_shelter_r47_80187284,
};

static TaskDesc D_shelter_r47_801872F0 = { { { TASK_BODY_NONE, 192 } }, shelterR47ConsolePromptTask, { .value = 0 } };

u8 D_shelter_r47_801872FC[8] = {
    0,
    13,
    20,
    30,
    39,
    48,
    57,
    255,
};

u8 D_shelter_r47_80187304[8] = {
    0,
    13,
    22,
    30,
    40,
    255,
    0,
    0,
};

u8 D_shelter_r47_8018730C[12] = {
    0,
    10,
    14,
    24,
    33,
    41,
    50,
    55,
    59,
    70,
    79,
    255,
};

u8 D_shelter_r47_80187318[4] = {
    255,
    0,
    0,
    0,
};

u8 D_shelter_r47_8018731C[12] = {
    0,
    11,
    14,
    22,
    30,
    35,
    39,
    49,
    58,
    65,
    255,
    0,
};

u8 D_shelter_r47_80187328[12] = {
    0,
    11,
    14,
    22,
    30,
    35,
    39,
    49,
    58,
    255,
    0,
    0,
};

u8 D_shelter_r47_80187334[16] = {
    0,
    11,
    21,
    32,
    41,
    50,
    59,
    63,
    67,
    76,
    79,
    90,
    98,
    107,
    255,
    0,
};

u8 D_shelter_r47_80187344[16] = {
    0,
    11,
    21,
    32,
    41,
    50,
    59,
    63,
    67,
    76,
    79,
    90,
    98,
    107,
    255,
    0,
};

u8 D_shelter_r47_80187354[16] = {
    0,
    12,
    20,
    30,
    39,
    49,
    52,
    60,
    70,
    74,
    78,
    88,
    98,
    255,
    0,
    0,
};

u8 D_shelter_r47_80187364[16] = {
    0,
    12,
    20,
    30,
    39,
    49,
    52,
    60,
    70,
    74,
    78,
    88,
    96,
    104,
    255,
    0,
};

u8* D_shelter_r47_80187374[10] = {
    D_shelter_r47_801872FC,
    D_shelter_r47_80187304,
    D_shelter_r47_8018730C,
    D_shelter_r47_80187318,
    D_shelter_r47_8018731C,
    D_shelter_r47_80187328,
    D_shelter_r47_80187334,
    D_shelter_r47_80187344,
    D_shelter_r47_80187354,
    D_shelter_r47_80187364,
};

ActionPromptHotspot D_shelter_r47_8018739C[5] = {
    { -156, -103, 78, 10, SHELTER_R47_MAP_HOTSPOT_TITLE, 0, 0 },
    { -150, 63, 56, 14, SHELTER_R47_MAP_HOTSPOT_PREV, 1, 0 },
    { -144, 80, 56, 14, SHELTER_R47_MAP_HOTSPOT_NEXT, 1, 0 },
    { -155, -90, 68, 84, SHELTER_R47_MAP_HOTSPOT_PANEL, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

ActionPromptHotspot D_shelter_r47_801873D8[3] = {
    { -150, 63, 56, 14, SHELTER_R47_MAP_HOTSPOT_PREV, 1, 0 },
    { -144, 80, 56, 14, SHELTER_R47_MAP_HOTSPOT_NEXT, 1, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

u8 D_shelter_r47_801873FC[8] = {
    37,
    38,
    39,
    40,
    41,
    0,
    0,
    0,
};

static const char Telephone_Data_8017D638[];

#include "../../shared/telephone.inc.c"

void shelterR47TelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// The three states of the room's main task, run by `shelterR47RoomTask`:
/// set-up, the per-frame handler and the kill.
static const TaskFuncTable3 D_shelter_r47_8017D6A4 = {
    {
        _shelterR47InitializeRoomTask,
        _shelterR47AdvanceTerminalTour,
        taskKill,
    },
};

/// Initializes the room's messages, terminal session and entry-event triggers.
///
/// Room state 0; registers the live room task, resets terminal handles and
/// first-use events, and spawns ambience. An arriving companion may be hidden
/// and given the room's entry animation. Disables the initial or repeat-entry
/// trigger according to story progress, then advances to state 1.
static void _shelterR47InitializeRoomTask(Task* task)
{
    enum { SHELTER_R47_INITIAL_ENTRY_TRIGGER   = 3,
           SHELTER_R47_REPEAT_ENTRY_TRIGGER    = 12,
           SHELTER_R47_AMBIENCE_TASK_INDEX     = 1,
           SHELTER_R47_COMPANION_ROUTE_STARTED = 1 };
    Task* companion;

    task->msgTable = D_shelter_r47_80186F2C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    companion = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (companion != NULL && gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 0 && gameFlagGetNibble(GAME_FLAG_0D1) == SHELTER_R47_COMPANION_ROUTE_STARTED) {
        taskMessageDispatch(companion, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
        companionWriteAnimationBankIndex(&D_shelter_r47_80186F5C.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(companion, ANIMATION_MESSAGE_PLAY, &D_shelter_r47_80186F5C, 0);
    }
    D_shelter_r47_8018A690 = NULL;
    shelterR47ResetTerminalFirstUseEvents();
    taskSpawnFromTable(D_shelter_r47_80186F70, SHELTER_R47_AMBIENCE_TASK_INDEX, 0, 0);
    // Only one entry event should remain available for this story branch.
    if (gameFlagGetNibble(GAME_FLAG_083) == 1 || gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 1) {
        WorldCollisionTrigger* initialEntryTrigger = &D_shelter_r47_8018787C[SHELTER_R47_INITIAL_ENTRY_TRIGGER];

        initialEntryTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    } else {
        WorldCollisionTrigger* repeatEntryTrigger = &D_shelter_r47_8018787C[SHELTER_R47_REPEAT_ENTRY_TRIGGER];

        repeatEntryTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    task->state++;
}

/// Advances the variant-1 event through the guided console, map tour and follow-up scenes.
///
/// Runs while the attachment wheel is closed. Event progress is saved in
/// `GAME_FLAG_SHELTER_R47_EVENT_PROGRESS`; idle event state permits opening
/// a terminal, and its released cutscene hold permits the following EVS scene.
/// Terminal handles are handed to the event scripts; allocation failures still
/// advance progress and hold the actors. `unusedTask` is ignored.
static void _shelterR47AdvanceTerminalTour(Task* unusedTask)
{
    enum {
        SHELTER_R47_TOUR_OPEN_CONSOLE     = 1,
        SHELTER_R47_TOUR_CONSOLE_FOLLOWUP = 2,
        SHELTER_R47_TOUR_OPEN_MAP         = 3,
        SHELTER_R47_TOUR_MAP_FOLLOWUP     = 4,
        SHELTER_R47_TOUR_DONE             = 5,
    };
    u8 variant = gGameSession->location.loc.variant;

    if (variant != 1 || Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return;
    }
    switch (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS)) {
        // Let each terminal release its hold before starting the next scripted phase.
        case SHELTER_R47_TOUR_OPEN_CONSOLE:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 1, 0);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                sceneSetPlacedActorDrawMode(0, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, SHELTER_R47_TOUR_CONSOLE_FOLLOWUP);
            }
            break;
        case SHELTER_R47_TOUR_CONSOLE_FOLLOWUP:
            if (gGameSession->cutsceneHold == 0) {
                evsStartScriptWithSkip(D_actor_443500_80141D9C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_443500_80142A74);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, SHELTER_R47_TOUR_OPEN_MAP);
            }
            break;
        case SHELTER_R47_TOUR_OPEN_MAP:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = taskSpawnFromTable(&D_shelter_r47_80187618, 0, SHELTER_R47_MAP_MODE_TOUR, 0);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                sceneSetPlacedActorDrawMode(0, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, SHELTER_R47_TOUR_MAP_FOLLOWUP);
            }
            break;
        case SHELTER_R47_TOUR_MAP_FOLLOWUP:
            if (gGameSession->cutsceneHold == 0) {
                evsStartScriptWithSkip(D_actor_443500_80142C24, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_443500_801432FC);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, SHELTER_R47_TOUR_DONE);
            }
            break;
    }
}

/// Replaces the automatic entry event with the repeat entry CAP action.
///
/// Disables trigger 3 and enables trigger 12, retaining their other flag bits.
/// Requires this overlay's thirteen-entry trigger table to remain loaded.
static inline void _shelterR47EnableRepeatEntryTrigger(void)
{
    enum { SHELTER_R47_INITIAL_ENTRY_TRIGGER = 3,
           SHELTER_R47_REPEAT_ENTRY_TRIGGER  = 12 };
    WorldCollisionTrigger* initialEntryTrigger;
    WorldCollisionTrigger* repeatEntryTrigger;

    initialEntryTrigger         = &D_shelter_r47_8018787C[SHELTER_R47_INITIAL_ENTRY_TRIGGER];
    repeatEntryTrigger          = &D_shelter_r47_8018787C[SHELTER_R47_REPEAT_ENTRY_TRIGGER];
    initialEntryTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    repeatEntryTrigger->flags  |= WORLD_COLLISION_TRIGGER_ENABLED;
}

/// Hides the tour terminal's actors and holds the player until terminal exit.
///
/// The player enters scripted idle; placed actor 0 receives only a hide request.
/// Requires the live player, its animation bank, the scene manager and a
/// successfully opened terminal already stored in the room's terminal handle.
/// Keep that handle and this room's restoration callback live until polling
/// finishes. The restoration task is spawned after the presentation changes;
/// allocation failure leaves the player held and both actors hidden.
static inline void _shelterR47HideActorsForTourTerminal(void)
{
    enum { SHELTER_R47_RESTORE_ACTORS_TASK_INDEX = 0,
           SHELTER_R47_TOUR_PLACED_ACTOR_INDEX   = 0,
           SHELTER_R47_PLACED_ACTOR_DRAW_HIDE    = 0 };

    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    sceneSetPlacedActorDrawMode(SHELTER_R47_TOUR_PLACED_ACTOR_INDEX, SHELTER_R47_PLACED_ACTOR_DRAW_HIDE);
    taskSpawnFromTable(D_shelter_r47_80186F70, SHELTER_R47_RESTORE_ACTORS_TASK_INDEX, 0, 0);
}

/// Holds and hides the player and any companion until the terminal exits.
///
/// Requires their live models and animation banks, and a successfully opened
/// terminal already stored in the room's terminal handle. Keep that handle and
/// this room's restoration callback live until polling finishes. Restoration
/// is spawned after both draw and scripted-idle requests; allocation failure
/// leaves the affected actors hidden and held.
static inline void _shelterR47HoldTerminalActors(void)
{
    enum { SHELTER_R47_RESTORE_ACTORS_TASK_INDEX = 0 };

    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
    taskSpawnFromTable(D_shelter_r47_80186F70, SHELTER_R47_RESTORE_ACTORS_TASK_INDEX, 0, 0);
}

/// Handles entry events, tour dialogue and terminal use requested by room triggers.
///
/// Borrows a four-byte `DirectionActionRequest` until return; only `actionId`
/// is read. Variant 1 advances the guided terminal tour, variant 2 starts the
/// companion entry event, and other variants allow ordinary terminal use.
/// Actor holds and a restoration task start only after a terminal spawn succeeds.
/// Requires this room's scripts, task descriptors and trigger table to remain
/// loaded. Other arguments are ignored, and every request returns zero.
static s32 _shelterR47HandleDirectionAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        SHELTER_R47_VARIANT_TERMINAL_TOUR          = 1,
        SHELTER_R47_VARIANT_COMPANION_ENTRY        = 2,
        SHELTER_R47_ACTION_COMPANION_ENTRY         = 1,
        SHELTER_R47_ACTION_POINT_2_SOUND           = 2,
        SHELTER_R47_ACTION_BEGIN_TERMINAL_TOUR     = 3,
        SHELTER_R47_ACTION_MAP_OR_TOUR_FOLLOWUP    = 4,
        SHELTER_R47_ACTION_OPEN_MAP                = 5,
        SHELTER_R47_ACTION_OPEN_CONSOLE            = 6,
        SHELTER_R47_OBJECTIVE_COMPANION_ENTRY      = 0x2A,
        SHELTER_R47_OBJECTIVE_TERMINAL_TOUR        = 0x2B,
        SHELTER_R47_DIALOGUE_COMPANION_ENTRY       = 4,
        SHELTER_R47_DIALOGUE_TERMINAL_TOUR         = 5,
        SHELTER_R47_COMPANION_SCHEDULE_AFTER_ENTRY = 8,
        SHELTER_R47_COMPANION_PROGRESS_AFTER_ENTRY = 2,
        SHELTER_R47_TOUR_NOT_STARTED               = 0,
        SHELTER_R47_TOUR_OPEN_CONSOLE              = 1,
        SHELTER_R47_TOUR_MAP_FOLLOWUP              = 4,
        SHELTER_R47_POINT_2_SOUND                  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1),
        SHELTER_R47_POINT_2_SOUND_PAN_OFFSET       = -10,
        SHELTER_R47_POINT_2_SOUND_ATTENUATION      = 64,
    };
    Task* tourMapTask;
    Task* tourConsoleTask;
    Task* consoleTask;
    Task* companionMapTask;
    Task* mapTask;
    Task* companionConsoleTask;
    u8    variant;

    variant = gGameSession->location.loc.variant;
    // The tour branch hides the placed scene actor rather than the companion.
    if (variant == SHELTER_R47_VARIANT_TERMINAL_TOUR) {
        switch (request->actionId) {
            case SHELTER_R47_ACTION_POINT_2_SOUND:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_POINT_2_SOUND_PLAYED) == 0)) {
                    sndEvtRequestScriptStart(SHELTER_R47_POINT_2_SOUND, SHELTER_R47_POINT_2_SOUND_PAN_OFFSET, SHELTER_R47_POINT_2_SOUND_ATTENUATION);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_POINT_2_SOUND_PLAYED, 1);
                }
                break;
            case SHELTER_R47_ACTION_BEGIN_TERMINAL_TOUR:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS) == SHELTER_R47_TOUR_NOT_STARTED)) {
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_R47_OBJECTIVE_TERMINAL_TOUR);
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, SHELTER_R47_DIALOGUE_TERMINAL_TOUR);
                    evsStartScriptWithSkip(D_actor_443500_8014152C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_443500_80141C1C);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, SHELTER_R47_TOUR_OPEN_CONSOLE);
                    if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_165) == 0) {
                        gameFlagSetNibble(GAME_FLAG_SHELTER_R47_165, 1);
                    }
                    _shelterR47EnableRepeatEntryTrigger();
                }
                break;
            case SHELTER_R47_ACTION_MAP_OR_TOUR_FOLLOWUP:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS) >= SHELTER_R47_TOUR_MAP_FOLLOWUP)) {
                    evsStartScript(D_actor_443500_80143494, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
                break;
            case SHELTER_R47_ACTION_OPEN_MAP:
                tourMapTask            = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = tourMapTask;
                if (tourMapTask != NULL) {
                    _shelterR47HideActorsForTourTerminal();
                }
                break;
            case SHELTER_R47_ACTION_OPEN_CONSOLE:
                tourConsoleTask        = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = tourConsoleTask;
                if (tourConsoleTask != NULL) {
                    _shelterR47HideActorsForTourTerminal();
                }
                break;
        }
    } else if (variant == SHELTER_R47_VARIANT_COMPANION_ENTRY) {
        switch (request->actionId) {
            case SHELTER_R47_ACTION_COMPANION_ENTRY:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 0) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 0)) {
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
                        evsStartScriptWithSkip(D_actor_143400_801350BC, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_143400_801359D4);
                    }
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_R47_OBJECTIVE_COMPANION_ENTRY);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_080, 1);
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, SHELTER_R47_COMPANION_SCHEDULE_AFTER_ENTRY);
                    gameFlagSetNibble(GAME_FLAG_0D1, SHELTER_R47_COMPANION_PROGRESS_AFTER_ENTRY);
                    companionRestoreFullHp();
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, SHELTER_R47_DIALOGUE_COMPANION_ENTRY);
                    _shelterR47EnableRepeatEntryTrigger();
                }
                break;
            case SHELTER_R47_ACTION_MAP_OR_TOUR_FOLLOWUP:
            case SHELTER_R47_ACTION_OPEN_MAP:
                companionMapTask       = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = companionMapTask;
                if (companionMapTask != NULL) {
                    _shelterR47HoldTerminalActors();
                }
                break;
            case SHELTER_R47_ACTION_OPEN_CONSOLE:
                companionConsoleTask   = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = companionConsoleTask;
                if (companionConsoleTask != NULL) {
                    _shelterR47HoldTerminalActors();
                }
                break;
        }
    } else {
        switch (request->actionId) {
            case SHELTER_R47_ACTION_MAP_OR_TOUR_FOLLOWUP:
            case SHELTER_R47_ACTION_OPEN_MAP:
                mapTask                = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = mapTask;
                if (mapTask != NULL) {
                    _shelterR47HoldTerminalActors();
                }
                break;
            case SHELTER_R47_ACTION_OPEN_CONSOLE:
                consoleTask            = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = consoleTask;
                if (consoleTask != NULL) {
                    _shelterR47HoldTerminalActors();
                }
                break;
        }
    }
    return 0;
}

/// Handles the room's cutscene and repeat-entry commands, returning zero.
///
/// Command 1 plays the first-use CAP command once; later uses run the view-44
/// cutscene, or command 42 while packed object state 34 is 1. Command 8 selects
/// entry scene 8 or 7 from the story branch. Other commands do nothing.
/// The cutscene task borrows the singleton record until it finishes, so callers
/// must serialize requests and keep this overlay and its loaded CAP resources live.
/// The task, message ID and second payload are ignored.
static s32 _shelterR47HandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedMode)
{
    enum {
        SHELTER_R47_COMMAND_CUTSCENE             = 1,
        SHELTER_R47_COMMAND_ENTRY_SCENE          = 8,
        SHELTER_R47_CUTSCENE_VIEW                = 44,
        SHELTER_R47_CUTSCENE_RESOURCE            = 3,
        SHELTER_R47_CUTSCENE_FOLLOWUP_COMMAND    = 10,
        SHELTER_R47_CUTSCENE_UNAVAILABLE_COMMAND = 42,
        SHELTER_R47_CUTSCENE_GATE_OBJECT         = 34,
        SHELTER_R47_FIRST_USE_CAP_COMMAND        = 1,
        SHELTER_R47_CUTSCENE_START_SOUND         = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 12),
        SHELTER_R47_CUTSCENE_END_SOUND           = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 15),
        SHELTER_R47_CUTSCENE_SCENE_SOUND         = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 13),
        SHELTER_R47_CUTSCENE_AFTER_SCENE_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 14),
    };
    if (command == SHELTER_R47_COMMAND_CUTSCENE) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_FIRST_USE) != 0) {
            if (areaGetCurrentObjectState(SHELTER_R47_CUTSCENE_GATE_OBJECT) == command) {
                capRunCommandWithTransition(SHELTER_R47_CUTSCENE_UNAVAILABLE_COMMAND);
                return 0;
            }
            D_shelter_r47_8018A698.view            = SHELTER_R47_CUTSCENE_VIEW;
            D_shelter_r47_8018A698.capSlot         = command;
            D_shelter_r47_8018A698.capFile         = SHELTER_R47_CUTSCENE_RESOURCE;
            D_shelter_r47_8018A698.skipScene       = 0;
            D_shelter_r47_8018A698.startSound      = SHELTER_R47_CUTSCENE_START_SOUND;
            D_shelter_r47_8018A698.endSound        = SHELTER_R47_CUTSCENE_END_SOUND;
            D_shelter_r47_8018A698.sceneSound      = SHELTER_R47_CUTSCENE_SCENE_SOUND;
            D_shelter_r47_8018A698.afterSceneSound = SHELTER_R47_CUTSCENE_AFTER_SCENE_SOUND;
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, SHELTER_R47_CUTSCENE_FOLLOWUP_COMMAND, &D_shelter_r47_8018A698);
        } else {
            gameFlagSetNibble(GAME_FLAG_SHELTER_R47_FIRST_USE, 1);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_r47_80186F70, 2, SHELTER_R47_FIRST_USE_CAP_COMMAND, 0);
        }
    } else if (command == SHELTER_R47_COMMAND_ENTRY_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_083) > 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_r47_80186F94, 0, 0, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) > 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_r47_80186F94, 1, 0, 0);
        }
    }
    return 0;
}

/// Maintains room ambience across event holds and camera views.
///
/// State 0 starts the loop; state 1 stops it on entering an event and restarts
/// or remixes it for views 2..4, stopping it in view 5. `spawnArg1.value` stores
/// the previous view and `killCountdown` stores the previous event state;
/// neither slot has its ordinary spawn/countdown meaning in this task.
static void _shelterR47AmbienceTask(Task* task)
{
    enum {
        SHELTER_R47_AMBIENCE_START              = 0,
        SHELTER_R47_AMBIENCE_UPDATE             = 1,
        SHELTER_R47_AMBIENCE_FADE_TICKS         = 60,
        SHELTER_R47_AMBIENCE_VIEW_4_PAN         = 12,
        SHELTER_R47_AMBIENCE_VIEW_4_ATTENUATION = 88,
    };
    switch (task->state) {
        case SHELTER_R47_AMBIENCE_START:
            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0, 0);
            task->spawnArg1.value = gGameSession->location.loc.view;
            task->killCountdown   = gGameSession->eventState;
            task->state++;
            break;
        case SHELTER_R47_AMBIENCE_UPDATE:
            // Event edges suspend/restart ambience; ready views update its spatial mix.
            if (gGameSession->eventState != task->killCountdown) {
                if (gGameSession->eventState != 0) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_R47_AMBIENCE, SHELTER_R47_AMBIENCE_FADE_TICKS);
                } else {
                    switch (gGameSession->location.loc.view) {
                        case 2:
                        case 3:
                            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0, 0);
                            break;
                        case 4:
                            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, SHELTER_R47_AMBIENCE_VIEW_4_PAN, SHELTER_R47_AMBIENCE_VIEW_4_ATTENUATION);
                            break;
                    }
                }
            } else {
                if (gGameSession->eventState == 0 && gGameSession->viewReady != 0) {
                    switch (gGameSession->location.loc.view) {
                        case 2:
                        case 3:
                            sndEvtRequestScriptMix(SOUND_SHELTER_R47_AMBIENCE, 0, 0);
                            break;
                        case 4:
                            if (task->spawnArg1.value == 3) {
                                sndEvtRequestScriptMix(SOUND_SHELTER_R47_AMBIENCE, SHELTER_R47_AMBIENCE_VIEW_4_PAN, SHELTER_R47_AMBIENCE_VIEW_4_ATTENUATION);
                            } else {
                                sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, SHELTER_R47_AMBIENCE_VIEW_4_PAN, SHELTER_R47_AMBIENCE_VIEW_4_ATTENUATION);
                            }
                            break;
                        case 5:
                            sndEvtRequestScriptStop(SOUND_SHELTER_R47_AMBIENCE, SHELTER_R47_AMBIENCE_FADE_TICKS);
                            break;
                    }
                }
                task->spawnArg1.value = gGameSession->location.loc.view;
            }
            task->killCountdown = gGameSession->eventState;
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses every room key-item-use request without changing state.
static s32 _shelterR47RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Copies and resolves a borrowed room-transition request, returning one.
///
/// `reply` must be writable for a complete `RoomEventMsg`; it may equal
/// `request`. The map overlay's variant resolver must remain loaded for the call.
/// Neither record is retained. The task and message ID are ignored.
static s32 _shelterR47ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return 1;
}

/// Starts this room's sound entry 17 for cutscene sound command 99.
///
/// Other commands do nothing; all requests return zero. The task, message ID
/// and second payload are ignored.
static s32 _shelterR47HandleSoundCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        SHELTER_R47_SOUND_COMMAND_CUTSCENE = 99,
        SHELTER_R47_CUTSCENE_SOUND         = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 17),
    };
    if (command == SHELTER_R47_SOUND_COMMAND_CUTSCENE) {
        sndEvtRequestScriptStart(SHELTER_R47_CUTSCENE_SOUND, 0, 0);
    }
    return 0;
}

/// Plays a command from the room's primary loaded CAP resource, then releases the player.
///
/// `spawnArg1.value` is the command index (the first-use caller supplies 1).
/// States start playback, wait, then reset CAP and kill the task. The caller
/// holds player control and keeps the room's CAP resources loaded until release.
static void _shelterR47PlayCapCommandTask(Task* task)
{
    switch (task->state) {
        case SHELTER_R47_CAP_START:
            Gp_CapFile = 0;
            capSelectLoadedFile(SHELTER_R47_CAP_MAIN_RESOURCE);
            capSetTexturePage(SHELTER_R47_CAP_MAIN_TEXTURE_X, SHELTER_R47_CAP_TEXTURE_Y);
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state++;
            break;
        case SHELTER_R47_CAP_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case SHELTER_R47_CAP_RELEASE:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Restores actors and player control after a terminal requests its exit.
///
/// Requires a live non-NULL terminal in `D_shelter_r47_8018A690` with its exit
/// handler loaded. Successful polling dispatches that exit before restoring the
/// actors, clears the borrowed handle and kills this polling task. Variant 1
/// also restores placed actor 0; a live companion is shown and released too.
static void _shelterR47RestoreActorsAfterTerminalTask(Task* task)
{
    s32 terminalResult;

    if (taskPollKill(D_shelter_r47_8018A690, &terminalResult) != 0) {
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        if (gGameSession->location.loc.variant == 1) {
            sceneSetPlacedActorDrawMode(0, 1);
        }
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        }
        D_shelter_r47_8018A690 = NULL;
        taskKill(task);
    }
}

void shelterR47RoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_r47_8017D6A4;
    states.funcs[task->state](task);
}

/// Plays primary-resource entry scene 8, then releases the player and counts the visit.
///
/// Enter at state 0 with player control held and the room's CAP data loaded.
/// Completion resets CAP and increments `GAME_FLAG_SHELTER_R47_165` up to 3.
/// State 1 falls through to task teardown after releasing control.
static void _shelterR47PlayEntryScene8Task(Task* task)
{
    enum { SHELTER_R47_ENTRY_COMMAND     = 8,
           SHELTER_R47_ENTRY_VISIT_LIMIT = 3 };
    s32 visitCount;

    switch (task->state) {
        case SHELTER_R47_CAP_START:
            capReset();
            Gp_CapFile = 0;
            capSelectLoadedFile(SHELTER_R47_CAP_MAIN_RESOURCE);
            capSetTexturePage(SHELTER_R47_CAP_MAIN_TEXTURE_X, SHELTER_R47_CAP_TEXTURE_Y);
            capRunCommandWithTransition(SHELTER_R47_ENTRY_COMMAND);
            task->state++;
            break;
        case SHELTER_R47_CAP_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            capReset();
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            visitCount = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_165);
            if (visitCount < SHELTER_R47_ENTRY_VISIT_LIMIT) {
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_165, visitCount + 1);
            }
        default:
            taskKill(task);
            break;
    }
}

/// Plays alternate-resource entry scene 7, then releases the player.
///
/// Enter at state 0 with player control held and the room's CAP data loaded.
/// State 1 resets CAP and falls through to task teardown after releasing control.
static void _shelterR47PlayEntryScene7Task(Task* task)
{
    enum { SHELTER_R47_ALTERNATE_ENTRY_COMMAND = 7 };
    switch (task->state) {
        case SHELTER_R47_CAP_START:
            capReset();
            Gp_CapFile = 0;
            capSelectLoadedFile(SHELTER_R47_CAP_ALTERNATE_RESOURCE);
            capSetTexturePage(SHELTER_R47_CAP_ALTERNATE_TEXTURE_X, SHELTER_R47_CAP_TEXTURE_Y);
            capRunCommandWithTransition(SHELTER_R47_ALTERNATE_ENTRY_COMMAND);
            task->state++;
            break;
        case SHELTER_R47_CAP_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            capReset();
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        default:
            taskKill(task);
            break;
    }
}

/// Draws the console's subtractive radial wipe as a 32-triangle disc.
///
/// Borrows the work unchanged; each colour component must be in 0..255.
/// `fillFromEdge` selects a grey centre and RGB rim; false exchanges them.
/// The disc is centred at the draw origin (0, 0), radius 256 pixels, with 4096
/// angle units per turn. Requires a word-aligned frame arena with space for
/// 32 `POLY_G3` and 32 `DR_MODE` reservations; advances `gGpuPrimCursor` and
/// links them into OT slot 11. Packet storage stays live until GPU completion.
static inline void _shelterR47ConsoleDrawWipe(const ShelterR47ConsoleWork* work, bool fillFromEdge)
{
    POLY_G3* triangle;
    DR_MODE* drawMode;
    s32      edgeAngle;
    s16      segment;

    for (segment = 0; segment < SHELTER_R47_CONSOLE_WIPE_SEGMENTS; segment++) {
        triangle       = gGpuPrimCursor;
        gGpuPrimCursor = triangle + 1;
        setPolyG3(triangle);
        if (fillFromEdge) {
            setRGB0(triangle, work->wipeGrey, work->wipeGrey, work->wipeGrey);
            setRGB1(triangle, work->wipeRed, work->wipeGreen, work->wipeBlue);
            setRGB2(triangle, work->wipeRed, work->wipeGreen, work->wipeBlue);
        } else {
            setRGB0(triangle, work->wipeRed, work->wipeGreen, work->wipeBlue);
            setRGB1(triangle, work->wipeGrey, work->wipeGrey, work->wipeGrey);
            setRGB2(triangle, work->wipeGrey, work->wipeGrey, work->wipeGrey);
        }
        setSemiTrans(triangle, 1);
        edgeAngle    = segment << SHELTER_R47_CONSOLE_WIPE_ANGLE_SHIFT;
        triangle->x0 = 0;
        triangle->y0 = 0;
        triangle->x1 = rsin(edgeAngle) >> SHELTER_R47_CONSOLE_WIPE_TRIG_SHIFT;
        triangle->y1 = rcos(edgeAngle) >> SHELTER_R47_CONSOLE_WIPE_TRIG_SHIFT;
        edgeAngle   += (1 << SHELTER_R47_CONSOLE_WIPE_ANGLE_SHIFT);
        triangle->x2 = rsin(edgeAngle) >> SHELTER_R47_CONSOLE_WIPE_TRIG_SHIFT;
        triangle->y2 = rcos(edgeAngle) >> SHELTER_R47_CONSOLE_WIPE_TRIG_SHIFT;
        addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_WIPE_OT], triangle);
        // Prepending the draw mode after its triangle makes subtraction execute first.
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));
        addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_WIPE_OT], drawMode);
    }
}

s16 shelterR47ConsoleClearWipe(Task* task)
{
    ShelterR47ConsoleWork* work;
    s32                    reachedEndpoint;

    work            = task->work;
    reachedEndpoint = 0;
    if (work->wipeGreen != 0) {
        work->wipeGreen -= SHELTER_R47_CONSOLE_WIPE_STEP;
        if (work->wipeGreen < 0) {
            work->wipeGreen = 0;
        }
    }
    if (work->wipeGreen < SHELTER_R47_CONSOLE_WIPE_CLEAR_NEXT) {
        if (work->wipeBlue != 0) {
            work->wipeBlue -= SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeBlue < 0) {
                work->wipeBlue = 0;
            }
        }
    }
    if (work->wipeBlue < SHELTER_R47_CONSOLE_WIPE_CLEAR_NEXT) {
        if (work->wipeRed != 0) {
            work->wipeRed -= SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeRed < 0) {
                work->wipeRed = 0;
            }
        }
    }
    if (work->wipeRed < SHELTER_R47_CONSOLE_WIPE_CLEAR_NEXT) {
        if (work->wipeGrey != 0) {
            work->wipeGrey -= SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeGrey < 0) {
                work->wipeGrey  = 0;
                reachedEndpoint = 1;
            }
        }
    }
    _shelterR47ConsoleDrawWipe(work, false);
    return reachedEndpoint;
}

s16 shelterR47ConsoleFillWipe(Task* task)
{
    ShelterR47ConsoleWork* work;
    s32                    reachedEndpoint;

    work            = task->work;
    reachedEndpoint = 0;
    if (work->wipeRed != SHELTER_R47_CONSOLE_WIPE_MAX) {
        work->wipeRed += SHELTER_R47_CONSOLE_WIPE_STEP;
        if (work->wipeRed >= (SHELTER_R47_CONSOLE_WIPE_MAX + 1)) {
            work->wipeRed = SHELTER_R47_CONSOLE_WIPE_MAX;
        }
    }
    if (work->wipeRed > SHELTER_R47_CONSOLE_WIPE_FILL_NEXT) {
        if (work->wipeBlue != SHELTER_R47_CONSOLE_WIPE_MAX) {
            work->wipeBlue += SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeBlue >= (SHELTER_R47_CONSOLE_WIPE_MAX + 1)) {
                work->wipeBlue = SHELTER_R47_CONSOLE_WIPE_MAX;
            }
        }
    }
    if (work->wipeBlue > SHELTER_R47_CONSOLE_WIPE_FILL_NEXT) {
        if (work->wipeGreen != SHELTER_R47_CONSOLE_WIPE_MAX) {
            work->wipeGreen += SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeGreen >= (SHELTER_R47_CONSOLE_WIPE_MAX + 1)) {
                work->wipeGreen = SHELTER_R47_CONSOLE_WIPE_MAX;
            }
        }
    }
    if (work->wipeGreen > SHELTER_R47_CONSOLE_WIPE_FILL_NEXT) {
        if (work->wipeGrey != SHELTER_R47_CONSOLE_WIPE_MAX) {
            work->wipeGrey += SHELTER_R47_CONSOLE_WIPE_STEP;
            if (work->wipeGrey >= (SHELTER_R47_CONSOLE_WIPE_MAX + 1)) {
                work->wipeGrey  = SHELTER_R47_CONSOLE_WIPE_MAX;
                reachedEndpoint = 1;
            }
        }
    }
    _shelterR47ConsoleDrawWipe(work, true);
    return reachedEndpoint;
}

void shelterR47ConsoleDrawSprite(s16 originX, s16 originY, s16 spriteId)
{
    enum {
        SHELTER_R47_CONSOLE_BUTTON_SPRITE       = 1,
        SHELTER_R47_CONSOLE_BUTTON_SPRITE_COUNT = 2,
        SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW   = 18,
        SHELTER_R47_CONSOLE_SPRITE_OT           = 10,
    };
    _ShelterR47SpritePart* part;
    POLY_FT4*              quad;

    part = D_shelter_r47_8018729C[spriteId];
    if (gGameSession->location.loc.view == SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW && (u16)(spriteId - SHELTER_R47_CONSOLE_BUTTON_SPRITE) < SHELTER_R47_CONSOLE_BUTTON_SPRITE_COUNT) {
        return;
    }
    while (part->clutX != SHELTER_R47_SPRITE_PART_END) {
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setUVWH(quad, part->u, part->v, part->w, part->h);
        quad->tpage = SHELTER_R47_SPRITE_PART_TPAGE;
        setShadeTex(quad, 1);
        quad->clut = getClut(part->clutX, part->clutY);
        setXYWH(quad, originX + part->x, originY + part->y, part->w, part->h);
        addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_SPRITE_OT], quad);
        part++;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Clears the console hotspot hit results before its next interaction.
///
/// Borrows a writable table ending in `ACTION_PROMPT_HOTSPOT_END`. Clears
/// every preceding entry's hit byte, including disabled hotspots, and leaves
/// the terminator and all other fields intact. Retains no pointer.
static inline void _shelterR47ConsoleClearHotspotHits(ActionPromptHotspot* hotspot)
{
    while (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
        hotspot->hit = 0;
        hotspot++;
    }
}

void shelterR47ConsoleInitializeTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_ENTRY_VIEW          = 16,
        SHELTER_R47_CONSOLE_BACKDROP_SWITCH     = 3,
        SHELTER_R47_CONSOLE_BACKDROP_OFF        = 0,
        SHELTER_R47_CONSOLE_BACKDROP_ON         = 1,
        SHELTER_R47_CONSOLE_BACKDROP_SCROLL_END = 320,
        SHELTER_R47_CONSOLE_INITIAL_WIPE_LEVEL  = 255,
        SHELTER_R47_CONSOLE_GUIDE_BEGIN         = 1
    };
    ShelterR47ConsoleWork* work;
    s32                    guideMode;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(&D_shelter_r47_801872F0, 0, 1, 0);
    task->work                                                 = work;
    work->savedView                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = SHELTER_R47_CONSOLE_ENTRY_VIEW;
    task->state++;
    displayAcquireMenuHold();

    _shelterR47ConsoleClearHotspotHits(D_shelter_r47_80186FB4);

    // Start the console panels off screen before their entrance animation.
    work->headerX              = -0xF8;
    work->headerY              = -0x68;
    work->buttonX              = -0x98;
    work->buttonY              = 0x80;
    work->messageX             = -0x98;
    work->messageY             = 0x90;
    work->labelX               = 0xB0;
    work->labelY               = -0x68;
    work->rowX[0]              = 0xAA;
    work->rowY[0]              = -0x53;
    work->rowX[1]              = 0xBE;
    work->rowY[1]              = -0x43;
    work->rowX[2]              = 0xD2;
    work->rowY[2]              = -0x33;
    work->rowX[3]              = 0xE6;
    work->rowY[3]              = -0x23;
    work->rowX[4]              = 0xFA;
    work->rowY[4]              = -0x13;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    shelterR47ConsoleLoadSwitches(task);
    if (work->toggles[SHELTER_R47_CONSOLE_BACKDROP_SWITCH] == 0) {
        work->backdropToggle = SHELTER_R47_CONSOLE_BACKDROP_OFF;
        work->backdropScroll = SHELTER_R47_CONSOLE_BACKDROP_SCROLL_END;
    } else {
        work->backdropToggle = SHELTER_R47_CONSOLE_BACKDROP_ON;
        work->backdropScroll = 0;
    }
    _shelterR47ConsoleSetWipe(task, SHELTER_R47_CONSOLE_INITIAL_WIPE_LEVEL);
    guideMode = task->spawnArg1.value;
    if (guideMode == SHELTER_R47_CONSOLE_GUIDE_BEGIN) {
        work->guideStep = guideMode;
    }
}

/// Latches a confirmed console hotspot and advances to its command prompt.
///
/// Borrows live task, console work and cursor records and a non-terminator
/// hotspot. Hides and stops the cursor, copies its choice and prompt kind,
/// and selects state 4. The caller has already tested and confirmed the hit;
/// this helper retains no pointer to the hotspot.
static inline void _shelterR47ConsoleAcceptHotspot(Task* task, ShelterR47ConsoleWork* work, ActionPrompt* prompt, const ActionPromptHotspot* hotspot)
{
    enum { SHELTER_R47_CONSOLE_STATE_OPEN_COMMANDS = 4 };

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->selection     = hotspot->id;
    work->promptKind    = hotspot->promptKind;
    task->state         = SHELTER_R47_CONSOLE_STATE_OPEN_COMMANDS;
}

void shelterR47ConsoleSelectHotspotTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_GUIDE_FREE           = 0,
        SHELTER_R47_CONSOLE_GUIDE_DONE           = 4,
        SHELTER_R47_CONSOLE_STATE_DISMISS        = 6,
        SHELTER_R47_CONSOLE_STATE_BEGIN_FADE_OUT = 12,
        SHELTER_R47_CONSOLE_CONFIRM_SLOT         = 0,
        SHELTER_R47_CONSOLE_CANCEL_SLOT          = 1
    };
    ShelterR47ConsoleWork* work;
    ActionPromptHotspot*   hotspot;
    ActionPrompt*          prompt;

    hotspot = D_shelter_r47_80186FB4;
    prompt  = D_80114D28;
    work    = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    if (work->guideStep == SHELTER_R47_CONSOLE_GUIDE_DONE) {
        task->state = SHELTER_R47_CONSOLE_STATE_BEGIN_FADE_OUT;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    // Overlapping hotspots can be hit together; confirmation takes the first.
    if (shelterR47ConsoleHitTestHotspots(task, hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[SHELTER_R47_CONSOLE_CONFIRM_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) && (hotspot->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hotspot->hit != 0) {
                    _shelterR47ConsoleAcceptHotspot(task, work, prompt, hotspot);
                    return;
                }
                hotspot++;
            } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (work->guideStep == SHELTER_R47_CONSOLE_GUIDE_FREE && prompt->buttons.slots[SHELTER_R47_CONSOLE_CANCEL_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = SHELTER_R47_CONSOLE_STATE_DISMISS;
    }
}
