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
/// Piece lists of the sprites `func_shelter_r47_80180F38` draws, by sprite id.
static _ShelterR47SpritePart* D_shelter_r47_8018729C[];
static TaskDesc               D_shelter_r47_801872F0;
static s32                    func_shelter_r47_8017FE84(Task*, s32, RoomEventMsg*, s32);
static s32                    func_shelter_r47_801801DC(Task*, s32, s32, s32);
static void                   func_shelter_r47_80180324(Task*);
static s32                    func_shelter_r47_801805D0(Task*, s32, s32, s32);
static s32                    func_shelter_r47_801805D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32                    func_shelter_r47_8018061C(Task*, s32, s32, s32);
static void                   func_shelter_r47_80180650(Task*);
static void                   func_shelter_r47_80180714(Task*);
static void                   func_shelter_r47_8018080C(Task*);
static void                   func_shelter_r47_801808D4(Task*);

#define TELEPHONE_TITLE_BYTES "Telephone\0\xDC" \
                              "2"
#include "../../shared/telephone.h"

static void func_shelter_r47_8017FB94(Task* task);
static void func_shelter_r47_8017FCC0(Task* task);

#include "../../shared/telephone_data.inc.c"

static TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static TaskMessageEntry D_shelter_r47_80186F2C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_r47_801805D8 },
    { 5105, func_shelter_r47_801805D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_r47_8017FE84 },
    { ROOM_MESSAGE_COMMAND, func_shelter_r47_801801DC },
    { ROOM_MESSAGE_SOUND, func_shelter_r47_8018061C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPlayRequest D_shelter_r47_80186F5C = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

static TaskDesc D_shelter_r47_80186F70[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_80180714, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_80180324, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_80180650, { .value = 0 } },
};

static TaskDesc D_shelter_r47_80186F94[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_8018080C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_801808D4, { .value = 0 } },
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

static TaskDesc D_shelter_r47_80187020 = { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_80182B18, { .value = 0 } };

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

static TaskDesc D_shelter_r47_801872F0 = { { { TASK_BODY_NONE, 192 } }, func_shelter_r47_80183234, { .value = 0 } };

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

void func_shelter_r47_8017EC04(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// The three states of the room's main task, run by `func_shelter_r47_801807B4`:
/// set-up, the per-frame handler and the kill.
static const TaskFuncTable3 D_shelter_r47_8017D6A4 = {
    {
        func_shelter_r47_8017FB94,
        func_shelter_r47_8017FCC0,
        taskKill,
    },
};

static void func_shelter_r47_8017FB94(Task* task)
{
    Task* player;

    task->msgTable = D_shelter_r47_80186F2C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    player = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (player != NULL && gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 0 && gameFlagGetNibble(GAME_FLAG_0D1) == 1) {
        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
        Gp_AllyAnimId(&D_shelter_r47_80186F5C.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_PLAY, &D_shelter_r47_80186F5C, 0);
    }
    D_shelter_r47_8018A690 = NULL;
    func_shelter_r47_80183210();
    taskSpawnFromTable(D_shelter_r47_80186F70, 1, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_083) == 1 || gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 1) {
        (D_shelter_r47_8018787C + 3)[0].flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    } else {
        {
            WorldCollisionTrigger* object = &D_shelter_r47_8018787C[12];
            object->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
        }
    }
    task->state++;
}

static void func_shelter_r47_8017FCC0(Task* task)
{
    u8 place = gGameSession->location.loc.variant;

    if (place != 1 || Gp_StateC08.mode == place) {
        return;
    }
    switch (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS)) {
        case 1:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 1, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, 2);
            }
            break;
        case 2:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634(D_actor_443500_80141D9C, 0, D_actor_443500_80142A74);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, 3);
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 2, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, 4);
            }
            break;
        case 4:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634(D_actor_443500_80142C24, 0, D_actor_443500_801432FC);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, 5);
            }
            break;
    }
}

static s32 func_shelter_r47_8017FE84(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    Task*                  spawned_p;
    Task*                  spawned_p6;
    Task*                  spawned_a;
    Task*                  spawned_a0;
    Task*                  spawned_a1;
    s32                    flag_a;
    s32                    flag_b;
    s32                    kind;
    u8                     field9;
    WorldCollisionTrigger* p;
    WorldCollisionTrigger* q;

    field9 = gGameSession->location.loc.variant;
    if (field9 == 1) {
        switch (arg2->warp) {
            case 2:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_POINT_2_SOUND_PLAYED) == 0)) {
                    sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), -0xA, 0x40);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_POINT_2_SOUND_PLAYED, 1);
                }
                break;
            case 3:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS) == 0)) {
                    func_800E3FAC(0xA2, 0x2B);
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 5);
                    func_800E8634(D_actor_443500_8014152C, 0, D_actor_443500_80141C1C);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS, 1);
                    if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_165) == 0) {
                        flag_a = 0x165;
                        flag_b = 1;
                        goto set_and_toggle;
                    }
                    goto toggle_only;
                }
                break;
            case 4:
                if ((gameFlagGetNibble(GAME_FLAG_083) == 1) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_EVENT_PROGRESS) >= 4)) {
                    func_800E8614(D_actor_443500_80143494, 0);
                }
                break;
            case 5:
                spawned_p              = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p;
                if (spawned_p != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    taskSpawnFromTable(D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
            case 6:
                spawned_p6             = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p6;
                if (spawned_p6 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    taskSpawnFromTable(D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    } else if (field9 == 2) {
        kind = arg2->warp;
        if (kind < 6) {
            if (kind < 4) {
                if ((kind == 1) && (gameFlagGetNibble(GAME_FLAG_083) == 0) && (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) == 0)) {
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
                        func_800E8634(D_actor_143400_801350BC, 0, D_actor_143400_801359D4);
                    }
                    func_800E3FAC(0xA2, 0x2A);
                    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_080, 1);
                    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 8);
                    gameFlagSetNibble(GAME_FLAG_0D1, 2);
                    Gp_FillAllyHp();
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                    flag_a = 0x155;
                    flag_b = 4;
                set_and_toggle:
                    gameFlagSetNibble(flag_a, flag_b);
                toggle_only:
                    p         = (D_shelter_r47_8018787C + 3);
                    q         = p + 9;
                    p->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
                    q->flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                }
            } else {
                spawned_a0             = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a0;
                if (spawned_a0 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    taskSpawnFromTable(D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            }
        } else {
            if (kind == 6) {
                goto spawn_six;
            }
        }
    } else {
        switch (arg2->warp) {
            case 4:
            case 5:
                spawned_a1             = taskSpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a1;
                if (spawned_a1 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    taskSpawnFromTable(D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            case 6:
            spawn_six:
                spawned_a              = taskSpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a;
                if (spawned_a != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    taskSpawnFromTable(D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    }
done:
    return 0;
}

/// Room request handler. Request 1 plays the room's cutscene through the shared
/// runner once flag 0x13E is set (or runs CAP command 0x2A instead while the
/// 2-bit flag 0x22 reads 1); the first time, it sets that flag and spawns entry
/// 2 of the room's task table. Request 8 spawns entry 0 or 1 of the second task
/// table, depending on which of flags 0x83 and 0x80 is set.
static s32 func_shelter_r47_801801DC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_FIRST_USE) != 0) {
            if (Gp_GetCurBit2Flag(0x22) == arg2) {
                Gp_RunCapCmd1(0x2A);
                return 0;
            }
            D_shelter_r47_8018A698.view            = 0x2C;
            D_shelter_r47_8018A698.capSlot         = arg2;
            D_shelter_r47_8018A698.capFile         = 3;
            D_shelter_r47_8018A698.skipScene       = 0;
            D_shelter_r47_8018A698.startSound      = 0x542F000C;
            D_shelter_r47_8018A698.endSound        = 0x542F000F;
            D_shelter_r47_8018A698.sceneSound      = 0x542F000D;
            D_shelter_r47_8018A698.afterSceneSound = 0x542F000E;
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 0xA, &D_shelter_r47_8018A698);
        } else {
            gameFlagSetNibble(GAME_FLAG_SHELTER_R47_FIRST_USE, 1);
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(D_shelter_r47_80186F70, 2, 1, 0);
        }
    } else if (arg2 == 8) {
        if (gameFlagGetNibble(GAME_FLAG_083) > 0) {
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(D_shelter_r47_80186F94, 0, 0, 0);
        } else if (gameFlagGetNibble(GAME_FLAG_SHELTER_R47_080) > 0) {
            Gp_MsgPlayerWeapon(0);
            taskSpawnFromTable(D_shelter_r47_80186F94, 1, 0, 0);
        }
    }
    return 0;
}

static void func_shelter_r47_80180324(Task* task)
{
    switch (task->state) {
        case 0:
            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0, 0);
            task->spawnArg1.value = gGameSession->location.loc.view;
            task->killCountdown   = gGameSession->eventState;
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != task->killCountdown) {
                if (gGameSession->eventState != 0) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_R47_AMBIENCE, 0x3C);
                } else {
                    switch (gGameSession->location.loc.view) {
                        case 2:
                        case 3:
                            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0, 0);
                            break;
                        case 4:
                            sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0xC, 0x58);
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
                                sndEvtRequestScriptMix(SOUND_SHELTER_R47_AMBIENCE, 0xC, 0x58);
                            } else {
                                sndEvtRequestScriptStart(SOUND_SHELTER_R47_AMBIENCE, 0xC, 0x58);
                            }
                            break;
                        case 5:
                            sndEvtRequestScriptStop(SOUND_SHELTER_R47_AMBIENCE, 0x3C);
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

static s32 func_shelter_r47_801805D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_shelter_80179A04`. Returns 1.
static s32 func_shelter_r47_801805D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

static s32 func_shelter_r47_8018061C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x63) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 0x11), 0, 0);
    }
    return 0;
}

static void func_shelter_r47_80180650(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(task->spawnArg1.value);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

static void func_shelter_r47_80180714(Task* task)
{
    s32 out;

    if (Task_PollKill(D_shelter_r47_8018A690, &out) != 0) {
        Gp_MsgPlayer3F3(1);
        Gp_MsgPlayerWeapon(1);
        if (gGameSession->location.loc.variant == 1) {
            Gp_MsgSlot4Chain(0, 1);
        }
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            Gp_MsgAlly3F3(1);
            Gp_MsgAllyWeapon(1);
        }
        D_shelter_r47_8018A690 = NULL;
        taskKill(task);
    }
}

void func_shelter_r47_801807B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r47_8017D6A4;
    sp.funcs[task->state](task);
}

static void func_shelter_r47_8018080C(Task* task)
{
    s32 nibble;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(8);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
            nibble = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_165);
            if (nibble < 3) {
                gameFlagSetNibble(GAME_FLAG_SHELTER_R47_165, nibble + 1);
            }
        default:
            taskKill(task);
            break;
    }
}

static void func_shelter_r47_801808D4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x140, 0x100);
            Gp_RunCapCmd1(7);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
        default:
            taskKill(task);
            break;
    }
}

s32 func_shelter_r47_8018097C(Task* task)
{
    ShelterR47ConsoleWork* work;
    POLY_G3*               tri;
    DR_MODE*               mode;
    s32                    angle;
    s32                    done;
    s16                    i;

    work = task->work;
    done = 0;
    if (work->wipeGreen != 0) {
        work->wipeGreen -= 0x20;
        if (work->wipeGreen < 0) {
            work->wipeGreen = 0;
        }
    }
    if (work->wipeGreen < 0xA0) {
        if (work->wipeBlue != 0) {
            work->wipeBlue -= 0x20;
            if (work->wipeBlue < 0) {
                work->wipeBlue = 0;
            }
        }
    }
    if (work->wipeBlue < 0xA0) {
        if (work->wipeRed != 0) {
            work->wipeRed -= 0x20;
            if (work->wipeRed < 0) {
                work->wipeRed = 0;
            }
        }
    }
    if (work->wipeRed < 0xA0) {
        if (work->wipeGrey != 0) {
            work->wipeGrey -= 0x20;
            if (work->wipeGrey < 0) {
                work->wipeGrey = 0;
                done           = 1;
            }
        }
    }
    for (i = 0; i < 0x20; i++) {
        tri            = gGpuPrimCursor;
        gGpuPrimCursor = tri + 1;
        setPolyG3(tri);
        setRGB0(tri, work->wipeRed, work->wipeGreen, work->wipeBlue);
        setRGB1(tri, work->wipeGrey, work->wipeGrey, work->wipeGrey);
        setRGB2(tri, work->wipeGrey, work->wipeGrey, work->wipeGrey);
        setSemiTrans(tri, 1);
        angle   = i << 7;
        tri->x0 = 0;
        tri->y0 = 0;
        tri->x1 = rsin(angle) >> 4;
        tri->y1 = rcos(angle) >> 4;
        angle  += 0x80;
        tri->x2 = rsin(angle) >> 4;
        tri->y2 = rcos(angle) >> 4;
        addPrim(&gGpuCurrentOt[11], tri);
        mode           = gGpuPrimCursor;
        gGpuPrimCursor = mode + 1;
        setlen(mode, 1);
        mode->code[0] = 0xE100004A;
        addPrim(&gGpuCurrentOt[11], mode);
    }
    return done;
}

s32 func_shelter_r47_80180C48(Task* task)
{
    ShelterR47ConsoleWork* work;
    POLY_G3*               tri;
    DR_MODE*               mode;
    s32                    angle;
    s32                    done;
    s16                    i;

    work = task->work;
    done = 0;
    if (work->wipeRed != 0xFF) {
        work->wipeRed += 0x20;
        if (work->wipeRed >= 0x100) {
            work->wipeRed = 0xFF;
        }
    }
    if (work->wipeRed > 0x60) {
        if (work->wipeBlue != 0xFF) {
            work->wipeBlue += 0x20;
            if (work->wipeBlue >= 0x100) {
                work->wipeBlue = 0xFF;
            }
        }
    }
    if (work->wipeBlue > 0x60) {
        if (work->wipeGreen != 0xFF) {
            work->wipeGreen += 0x20;
            if (work->wipeGreen >= 0x100) {
                work->wipeGreen = 0xFF;
            }
        }
    }
    if (work->wipeGreen > 0x60) {
        if (work->wipeGrey != 0xFF) {
            work->wipeGrey += 0x20;
            if (work->wipeGrey >= 0x100) {
                work->wipeGrey = 0xFF;
                done           = 1;
            }
        }
    }
    for (i = 0; i < 0x20; i++) {
        tri            = gGpuPrimCursor;
        gGpuPrimCursor = tri + 1;
        setPolyG3(tri);
        setRGB0(tri, work->wipeGrey, work->wipeGrey, work->wipeGrey);
        setRGB1(tri, work->wipeRed, work->wipeGreen, work->wipeBlue);
        setRGB2(tri, work->wipeRed, work->wipeGreen, work->wipeBlue);
        setSemiTrans(tri, 1);
        angle   = i << 7;
        tri->x0 = 0;
        tri->y0 = 0;
        tri->x1 = rsin(angle) >> 4;
        tri->y1 = rcos(angle) >> 4;
        angle  += 0x80;
        tri->x2 = rsin(angle) >> 4;
        tri->y2 = rcos(angle) >> 4;
        addPrim(&gGpuCurrentOt[11], tri);
        mode           = gGpuPrimCursor;
        gGpuPrimCursor = mode + 1;
        setlen(mode, 1);
        mode->code[0] = 0xE100004A;
        addPrim(&gGpuCurrentOt[11], mode);
    }
    return done;
}

/// Draws sprite `id` with its origin at (`x`, `y`), one raw-textured quad
/// per piece into OT slot 10. Sprites 1 and 2 are skipped while
/// the current view is 0x12.
void func_shelter_r47_80180F38(s16 x, s16 y, s16 id)
{
    _ShelterR47SpritePart* part;
    POLY_FT4*              p;

    part = D_shelter_r47_8018729C[id];
    if (gGameSession->location.loc.view == 0x12 && (u16)(id - 1) < 2) {
        return;
    }
    while (part->clutX != SHELTER_R47_SPRITE_PART_END) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setPolyFT4(p);
        setUVWH(p, part->u, part->v, part->w, part->h);
        p->tpage = SHELTER_R47_SPRITE_PART_TPAGE;
        setShadeTex(p, 1);
        p->clut = getClut(part->clutX, part->clutY);
        setXYWH(p, x + part->x, y + part->y, part->w, part->h);
        addPrim(&gGpuCurrentOt[10], p);
        part++;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

void func_shelter_r47_8018138C(Task* task)
{
    ShelterR47ConsoleWork* work;
    ActionPromptHotspot*   hs;
    s32                    arg1;

    work = memCalloc(0x54, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(&D_shelter_r47_801872F0, 0, 1, 0);
    task->work                                                 = work;
    work->savedView                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x10;
    task->state++;
    Display_AcquireRef();

    hs = D_shelter_r47_80186FB4;
    while (hs->id != ACTION_PROMPT_HOTSPOT_END) {
        hs->hit = 0;
        hs++;
    }

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
    func_shelter_r47_80182AA0(task);
    if (work->toggles[3] == 0) {
        work->backdropToggle = 0;
        work->backdropScroll = 0x140;
    } else {
        work->backdropToggle = 1;
        work->backdropScroll = 0;
    }
    {
        ShelterR47ConsoleWork* w = task->work;

        w->wipeRed   = 0xFF;
        w->wipeGreen = 0xFF;
        w->wipeBlue  = 0xFF;
        w->wipeGrey  = 0xFF;
    }
    arg1 = task->spawnArg1.value;
    if (arg1 == 1) {
        work->guideStep = arg1;
    }
}

/// Hotspot state of the room's first cap script: redraws the scene, then
/// hit-tests the action cursor against the room's hotspot table. A miss
/// leaves the idle cursor; a hit with the prompt confirmed
/// (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`) hands the raised entry's `id` / `promptKind` to the
/// work block and advances to state 4. `guideStep` value 4 jumps to state 0xC,
/// and with `guideStep` clear a dismissed prompt advances to state 6.
void func_shelter_r47_80181568(Task* task)
{
    ShelterR47ConsoleWork* work;
    ActionPromptHotspot*   hs;
    ActionPrompt*          prompt;

    hs     = D_shelter_r47_80186FB4;
    prompt = D_80114D28;
    work   = task->work;
    func_shelter_r47_80181914(task, 0);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    if (work->guideStep == 4) {
        task->state = 0xC;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (func_shelter_r47_80182B9C(task, hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hs->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->selection     = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 4;
                    return;
                }
                hs++;
            } while (hs->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (work->guideStep == 0 && prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 6;
    }
}
