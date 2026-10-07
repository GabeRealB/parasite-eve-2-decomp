#include "shelter_r47_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/pad_input.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gameflag_types.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"
#include "../../shared/action_prompt.h"

/// `_ShelterR47MapMark.stage` of the entry closing a marker table.
#define SHELTER_R47_MAP_MARK_END 0xFF

/// Where the room's map terminal draws the marker of one area.
///
/// Each page of the terminal's map has a table of these, closed by an entry
/// whose `stage` is `SHELTER_R47_MAP_MARK_END`. An entry's marker is an 8x8
/// sprite drawn over the page while the area's saved state has
/// `AREA_SAVED_MAP_MARK` set and `AREA_SPAWN_RESTORE_SAVED_POSES` clear, the
/// condition the map screen tests to mark the area's room.
typedef struct {
    s16 stage; // Stage the area belongs to, as `GameLocationKey.stage`, or `SHELTER_R47_MAP_MARK_END`
    u16 area;  // Area the marker stands for, as `GameLocationKey.area`
    s16 x;     // X of the marker's centre, in screen pixels from the display centre
    s16 y;     // Y of the marker's centre, in the same units
} _ShelterR47MapMark;
STATIC_ASSERT_SIZEOF(_ShelterR47MapMark, 8);

/// Marker tables indexed by `ShelterR47MapTerminalWork::page`. The second is used
/// while `GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED` is 1.
extern _ShelterR47MapMark* D_shelter_r47_801875C4[];
extern _ShelterR47MapMark* D_shelter_r47_801875D8[];

extern s16 D_shelter_r47_801875EC[];
extern s16 D_shelter_r47_801875F8[][2];

/// Console rows follow the saved-switch order, not the sprite UV order.
enum {
    SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR = 0,
    SHELTER_R47_CONSOLE_ROW_SWITCH_2      = 1,
    SHELTER_R47_CONSOLE_ROW_OBSERVATORY   = 2,
    SHELTER_R47_CONSOLE_ROW_SWITCH_4      = 3,
    SHELTER_R47_CONSOLE_ROW_WATCHERS      = 4,
};

/// Sprite ids in the console's composite sprite catalogue.
enum {
    SHELTER_R47_CONSOLE_STATUS_SPRITE_BASE = 3,
    SHELTER_R47_CONSOLE_MARKER_SPRITE      = 20,
};

/// Status index: two messages per switch, off followed by on.
enum {
    SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_OFF = 0,
    SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_ON  = 1,
    SHELTER_R47_CONSOLE_STATUS_SWITCH_2_OFF      = 2,
    SHELTER_R47_CONSOLE_STATUS_SWITCH_2_ON       = 3,
    SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_OFF   = 4,
    SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_ON    = 5,
    SHELTER_R47_CONSOLE_STATUS_SWITCH_4_OFF      = 6,
    SHELTER_R47_CONSOLE_STATUS_SWITCH_4_ON       = 7,
    SHELTER_R47_CONSOLE_STATUS_WATCHERS_OFF      = 8,
    SHELTER_R47_CONSOLE_STATUS_WATCHERS_ON       = 9,
};

/// Input timing, byte-scale wipe levels and GPU packet codes shared by the room.
enum {
    SHELTER_R47_CONSOLE_LEVEL_MAX                 = 255,
    SHELTER_R47_CONSOLE_INTERACTION_REARM_UPDATES = 10,
    SHELTER_R47_MAP_TERMINAL_LABEL_OT             = 11,
    SHELTER_R47_DRAW_MODE_COMMAND                 = 0xE1000000,
    SHELTER_R47_SPRITE_GPU_WORDS                  = 4,
    SHELTER_R47_SPRITE_COMMAND                    = 0x64,
};

static void _actionPromptResetDefault(Task* task);
static void func_shelter_r47_801816CC(Task* task);
static void _shelterR47ConsoleDrawStatusReveal(Task* task, s16 messageY);
static void _shelterR47ConsoleDrawScrollingBackdrop(s16 scrollPixels);
static void _shelterR47ConsoleFadeOutTask(Task* task);
static s16  func_shelter_r47_801829B8(Task* task, s16 arg1);
static void _shelterR47ConsoleResetPromptTask(Task* task);
static void func_shelter_r47_80182CA4(Task* task);
static void _shelterR47ConsoleOpenCommandsTask(Task* task);
static void _shelterR47ConsoleDismissTask(Task* task);
static void func_shelter_r47_80182F18(Task* task);
static void _shelterR47ConsoleChangeViewTask(Task* task);
static void _shelterR47ConsoleBeginButtonPressTask(Task* task);
static void _shelterR47ConsoleApplyButtonPressTask(Task* task);
static void _shelterR47ConsoleWaitButtonFlashTask(Task* task);
static void _shelterR47ConsoleBeginFadeOutTask(Task* task);
static void func_shelter_r47_801832E4(s16 status);
static void func_shelter_r47_801832EC(Task* task);
static void _shelterR47ConsoleSaveSwitches(Task* task);
static void _shelterR47ConsoleToggleSwitch(Task* task, s16 row);
static void func_shelter_r47_80183484(Task* task);

/// State handlers of the room's first cap script, run by
/// `func_shelter_r47_80182B18`.
static const TaskFuncTable14 D_shelter_r47_8017D6C8 = {
    {
        func_shelter_r47_8018138C,
        _shelterR47ConsoleResetPromptTask,
        func_shelter_r47_80182CA4,
        func_shelter_r47_80181568,
        _shelterR47ConsoleOpenCommandsTask,
        func_shelter_r47_801816CC,
        _shelterR47ConsoleDismissTask,
        _shelterR47ConsoleChangeViewTask,
        func_shelter_r47_80182F18,
        _shelterR47ConsoleBeginButtonPressTask,
        _shelterR47ConsoleApplyButtonPressTask,
        _shelterR47ConsoleWaitButtonFlashTask,
        _shelterR47ConsoleBeginFadeOutTask,
        _shelterR47ConsoleFadeOutTask,
    },
};

_ShelterR47MapMark D_shelter_r47_80187404[7] = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 124, 0 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 87, 0 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 55, 0 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_HELIPORT, 0, 0 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 100, -17 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, -57, 0 },
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark D_shelter_r47_8018743C[17] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 100, -77 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 64, -58 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 55, -12 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 51, 48 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 94, 13 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 130, -3 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 102, 40 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 86, 56 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, 120, 81 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 100, 93 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 24, 53 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 24, 25 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM, -15, 52 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1, -15, 36 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 5, 25 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, -41, -17 },
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark D_shelter_r47_801874C4[11] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 53, -73 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 13, -73 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 7, -11 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 11, 45 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_ELEVATOR_HALL, 20, 94 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 84, 78 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_OPERATING_ROOM, 67, 33 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 45, 59 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, 86, 2 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 56, 9 },
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark D_shelter_r47_8018751C[5] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, -33, -59 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 85, -53 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 89, 51 },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 99, 74 },
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark D_shelter_r47_80187544[1] = {
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark D_shelter_r47_8018754C[15] = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_EVE_ACCESS_TUNNEL, 146, 6 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_NORTH_PROMENADE, 135, -65 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 45, -75 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, -15, -75 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, -57, -78 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, -23, -44 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, -53, -36 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, -65, 24 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SUBSTATION, -21, 28 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, -21, 40 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, -29, 60 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 25, 79 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 84, 60 },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 136, 52 },
    { SHELTER_R47_MAP_MARK_END },
};

_ShelterR47MapMark* D_shelter_r47_801875C4[5] = {
    D_shelter_r47_8018743C,
    D_shelter_r47_801874C4,
    D_shelter_r47_8018751C,
    D_shelter_r47_80187544,
    D_shelter_r47_80187404,
};

_ShelterR47MapMark* D_shelter_r47_801875D8[5] = {
    D_shelter_r47_8018743C,
    D_shelter_r47_801874C4,
    D_shelter_r47_8018751C,
    D_shelter_r47_8018754C,
    D_shelter_r47_80187404,
};

s16 D_shelter_r47_801875EC[6] = {
    10,
    20,
    30,
    40,
    0,
    0,
};

s16 D_shelter_r47_801875F8[5][2] = {
    { 0, 16 },
    { 8, 24 },
    { 16, 32 },
    { 24, 0 },
    { 32, 8 },
};

static inline s32 _shelterR47GetAreaFlag4(GameLocationKey* key);
static inline s16 _shelterR47IsAreaMarked(s32 stage, s32 area);

/// Prepares one draw-mode/sprite packet for texture and geometry setup.
///
/// Requires a word-aligned writable `packet` and a complete E1 GPU command.
/// RGB is retained. Merging the adjacent primitives clears the sprite tag to
/// a no-op and makes six words follow the draw-mode tag. The caller supplies
/// texture flags, CLUT, UVs and geometry before linking the packet into an OT.
static inline void _shelterR47InitDrawModeSprite(SpriteDrawModePacket* packet, u32 drawModeCommand)
{
    setlen(&packet->drawMode, ARRAY_SIZE(packet->drawMode.code));
    setlen(&packet->sprite.sprt, SHELTER_R47_SPRITE_GPU_WORDS);
    packet->drawMode.code[0] = drawModeCommand;
    setcode(&packet->sprite.sprt, SHELTER_R47_SPRITE_COMMAND);
    MargePrim(packet, &packet->sprite.sprt);
}

/// Sets the console's circular wipe to a uniform clear-to-black level.
///
/// Requires live `ShelterR47ConsoleWork` and `level` in 0..255 (clear to black).
/// Reloads the task's work and stores all four signed-halfword components;
/// it does not step or draw the wipe, or change the task state.
static inline void _shelterR47ConsoleSetWipe(Task* task, s16 level)
{
    ShelterR47ConsoleWork* wipeWork = task->work;

    wipeWork->wipeRed   = level;
    wipeWork->wipeGreen = level;
    wipeWork->wipeBlue  = level;
    wipeWork->wipeGrey  = level;
}

/// Publishes a console status and queues its corresponding message sprite.
///
/// Requires `status` in 0..9 and live `ShelterR47ConsoleWork`. Coordinates are
/// read from `work` in display-centred pixels before reloading the task's work
/// and publishing its status. Statuses select sprites 3..12 in the room's
/// catalogue. Requires the textures, frame arena and OT of
/// `shelterR47ConsoleDrawSprite`; this does not advance the status reveal.
static inline void _shelterR47ConsoleDrawStatus(Task* task, ShelterR47ConsoleWork* work, s16 status)
{
    s16                    messageX    = work->messageX;
    s16                    messageY    = work->messageY;
    ShelterR47ConsoleWork* currentWork = task->work;

    currentWork->status = status;
    shelterR47ConsoleDrawSprite(messageX, messageY, SHELTER_R47_CONSOLE_STATUS_SPRITE_BASE + status);
}

/// Acts on `selection`, the hotspot id stored by `func_shelter_r47_80181568`,
/// when `itemMenuIsHotspotActionConfirmed` returns nonzero: the id's high byte picks the kind.
/// Kind 0 accepts a new low byte into `row` (checked by
/// `func_shelter_r47_801829B8` while `guideStep` is set, and the first time
/// gated by a one-off cap event otherwise), clears the wipe colour and
/// moves to state 7. Kind 1 moves to state 9 after its one-off event, kind 2
/// starts the cap event for the current `status`, and kinds 3 and 4 start their
/// own events. Every other outcome returns to state 3.
static void func_shelter_r47_801816CC(Task* task)
{
    ShelterR47ConsoleWork* work;
    ShelterR47ConsoleWork* w;
    ActionPrompt*          prompt;
    u32                    kind;

    prompt = D_80114D28;
    work   = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        kind = (u16)work->selection >> 8;
        if (kind == 0) {
            if (work->row != (work->selection & 0xFF)) {
                if (work->guideStep != 0) {
                    if (func_shelter_r47_801829B8(task, work->selection & 0xFF) == 0) {
                        task->state = 3;
                        return;
                    }
                    work->previousRow = work->row;
                    work->row         = work->selection;
                    w                 = task->work;
                    w->wipeRed        = 0;
                    w->wipeGreen      = 0;
                    w->wipeBlue       = 0;
                    w->wipeGrey       = 0;
                    task->state       = 7;
                    return;
                }
                if (D_shelter_r47_8018A695 == 0) {
                    capStartSequenceSlot(0x2E, 0, 0);
                    D_shelter_r47_8018A695 = 1;
                    task->state            = 3;
                    return;
                }
                work->previousRow = work->row;
                work->row         = work->selection;
                w                 = task->work;
                w->wipeRed        = 0;
                w->wipeGreen      = 0;
                w->wipeBlue       = 0;
                w->wipeGrey       = 0;
                task->state       = 7;
                return;
            }
            if (work->guideStep != 0) {
                capStartSequenceSlot(0xF, 0, 0);
            }
            task->state = 3;
            return;
        }
        if (kind == 1) {
            if (D_shelter_r47_8018A694 == 0) {
                capStartSequenceSlot(0x2C, 0, 0);
                D_shelter_r47_8018A694 = kind;
                task->state            = 3;
                return;
            }
            task->state = 9;
            return;
        }
        if (kind == 2) {
            switch (work->status) {
                case 0:
                    capStartSequenceSlot(0x15, 0, 0);
                    break;
                case 1:
                    capStartSequenceSlot(0x16, 0, 0);
                    break;
                case 2:
                    capStartSequenceSlot(0x17, 0, 0);
                    break;
                case 3:
                    capStartSequenceSlot(0x18, 0, 0);
                    break;
                case 4:
                    capStartSequenceSlot(0x1A, 0, 0);
                    break;
                case 5:
                    capStartSequenceSlot(0x19, 0, 0);
                    break;
                case 6:
                    capStartSequenceSlot(0x1B, 0, 0);
                    break;
                case 7:
                    capStartSequenceSlot(0x1C, 0, 0);
                    break;
                case 8:
                    capStartSequenceSlot(0x22, 0, 0);
                    break;
                case 9:
                    capStartSequenceSlot(0x23, 0, 0);
                    break;
            }
            task->state = 3;
            return;
        }
        if (kind == 3) {
            capStartSequenceSlot(0x2B, 0, 0);
            task->state = 3;
            return;
        }
        if (kind == 4) {
            capStartSequenceSlot(0x2D, 0, 0);
            task->state = 3;
            return;
        }
        return;
    }
    task->state = 3;
}

/// Sets the task's `status` to `st` and draws sprite `id` at (`messageX`,
/// `messageY`).
#define SHELTER_R47_DRAW_STEP(id, st)                                  \
    {                                                                  \
        s16 x_                                       = work->messageX; \
        s16 y_                                       = work->messageY; \
        ((ShelterR47ConsoleWork*)task->work)->status = (st);           \
        shelterR47ConsoleDrawSprite(x_, y_, (id));                     \
    }

void shelterR47ConsoleUpdateAndDraw(Task* task, s16 changingView)
{
    enum {
        SHELTER_R47_CONSOLE_HEADER_SPRITE            = 0,
        SHELTER_R47_CONSOLE_BUTTON_SPRITE            = 1,
        SHELTER_R47_CONSOLE_PRESSED_BUTTON_SPRITE    = 2,
        SHELTER_R47_CONSOLE_LABEL_SPRITE_BASE        = 13,
        SHELTER_R47_CONSOLE_ROW_SWITCH_SPRITE        = 18,
        SHELTER_R47_CONSOLE_ACTIVE_ROW_SWITCH_SPRITE = 19,
        SHELTER_R47_CONSOLE_SCROLLING_VIEW           = 20,
        SHELTER_R47_CONSOLE_SCROLL_MAX               = 320,
    };
    ShelterR47ConsoleWork* work;
    s32                    rowIndex;
    s16                    rowX;
    s32                    labelX;
    s32                    rowY;
    s16                    labelY;
    s8                     labelRow;
    s16                    labelRowIndex;

    work = task->work;
    // The row-3 view pans a backdrop wider than the display.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == SHELTER_R47_CONSOLE_SCROLLING_VIEW) {
        if (work->backdropToggle & 1) {
            work->backdropScroll--;
            if (work->backdropScroll < 0) {
                work->backdropScroll = 0;
            }
        } else {
            work->backdropScroll++;
            if (work->backdropScroll > SHELTER_R47_CONSOLE_SCROLL_MAX) {
                work->backdropScroll = SHELTER_R47_CONSOLE_SCROLL_MAX;
            }
        }
        _shelterR47ConsoleDrawScrollingBackdrop(work->backdropScroll);
    }
    // Ease the controls and reveal the status retained from the last draw.
    if (work->buttonFlash > 0) {
        work->buttonFlash--;
    }
    work->headerX += (-0x98 - work->headerX) >> 2;
    shelterR47ConsoleDrawSprite(work->headerX, work->headerY, SHELTER_R47_CONSOLE_HEADER_SPRITE);
    if (changingView == 0) {
        work->buttonY += (0x48 - work->buttonY) >> 2;
        if (work->buttonFlash == 0) {
            shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, SHELTER_R47_CONSOLE_BUTTON_SPRITE);
        } else {
            shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, SHELTER_R47_CONSOLE_PRESSED_BUTTON_SPRITE);
        }
        work->messageY += (0x58 - work->messageY) >> 2;
        _shelterR47ConsoleDrawStatusReveal(task, work->messageY);
        switch (work->row) {
            case SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR:
                if (work->toggles[0] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_SWITCH_2:
                if (work->toggles[1] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_2_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_2_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_OBSERVATORY:
                if (work->toggles[2] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_SWITCH_4:
                if (work->toggles[3] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_4_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_4_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_WATCHERS:
                if (work->toggles[4] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_WATCHERS_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_WATCHERS_ON);
                }
                break;
        }
    } else {
        work->buttonY += (0x80 - work->buttonY) >> 2;
        shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, SHELTER_R47_CONSOLE_BUTTON_SPRITE);
        work->messageY += (0x90 - work->messageY) >> 2;
        _shelterR47ConsoleDrawStatusReveal(task, work->messageY);
        switch (work->previousRow) {
            case SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR:
                if (work->toggles[0] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_SWITCH_2:
                if (work->toggles[1] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_2_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_2_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_OBSERVATORY:
                if (work->toggles[2] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_SWITCH_4:
                if (work->toggles[3] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_4_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_SWITCH_4_ON);
                }
                break;
            case SHELTER_R47_CONSOLE_ROW_WATCHERS:
                if (work->toggles[4] == 0) {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_WATCHERS_OFF);
                } else {
                    _shelterR47ConsoleDrawStatus(task, work, SHELTER_R47_CONSOLE_STATUS_WATCHERS_ON);
                }
                break;
        }
    }
    // Fan out unselected rows only while the scene view is changing.
    if (changingView == 0) {
        for (rowIndex = 0; rowIndex < (s32)ARRAY_SIZE(work->rowX); rowIndex++) {
            rowX                 = work->rowX[rowIndex] + ((0x78 - work->rowX[rowIndex]) >> 2);
            work->rowX[rowIndex] = rowX;
            if (work->row == rowIndex) {
                rowY = work->rowY[rowIndex];
                shelterR47ConsoleDrawSprite((s16)(rowX + 0x18), rowY, SHELTER_R47_CONSOLE_MARKER_SPRITE);
                shelterR47ConsoleDrawSprite(rowX, rowY, SHELTER_R47_CONSOLE_ROW_SWITCH_SPRITE);
            } else {
                shelterR47ConsoleDrawSprite(rowX, work->rowY[rowIndex], SHELTER_R47_CONSOLE_ROW_SWITCH_SPRITE);
            }
        }
    } else {
        for (rowIndex = 0; rowIndex < (s32)ARRAY_SIZE(work->rowX); rowIndex++) {
            if (work->row == rowIndex) {
                rowX                 = work->rowX[rowIndex] + ((0x78 - work->rowX[rowIndex]) >> 2);
                rowY                 = work->rowY[rowIndex];
                work->rowX[rowIndex] = rowX;
                shelterR47ConsoleDrawSprite((s16)(rowX + 0x18), rowY, SHELTER_R47_CONSOLE_MARKER_SPRITE);
                shelterR47ConsoleDrawSprite(rowX, rowY, SHELTER_R47_CONSOLE_ACTIVE_ROW_SWITCH_SPRITE);
            } else {
                switch (rowIndex) {
                    case SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR:
                        work->rowX[rowIndex] += (0xAA - work->rowX[rowIndex]) >> 2;
                        break;
                    case SHELTER_R47_CONSOLE_ROW_SWITCH_2:
                        work->rowX[rowIndex] += (0xBE - work->rowX[rowIndex]) >> 2;
                        break;
                    case SHELTER_R47_CONSOLE_ROW_OBSERVATORY:
                        work->rowX[rowIndex] += (0xD2 - work->rowX[rowIndex]) >> 2;
                        break;
                    case SHELTER_R47_CONSOLE_ROW_SWITCH_4:
                        work->rowX[rowIndex] += (0xE6 - work->rowX[rowIndex]) >> 2;
                        break;
                    case SHELTER_R47_CONSOLE_ROW_WATCHERS:
                        work->rowX[rowIndex] += (0xFA - work->rowX[rowIndex]) >> 2;
                        break;
                }
                shelterR47ConsoleDrawSprite(work->rowX[rowIndex], work->rowY[rowIndex], SHELTER_R47_CONSOLE_ROW_SWITCH_SPRITE);
            }
        }
    }
    if (changingView == 0) {
        labelRow = work->row;
        labelX   = work->labelX;
        labelY   = work->labelY;
    } else {
        labelRow = work->previousRow;
        labelX   = work->labelX;
        labelY   = work->labelY;
    }
    work->labelX += (0x7E - labelX) >> 2;
    labelRowIndex = labelRow;
    if ((u16)labelRowIndex < (s32)ARRAY_SIZE(work->rowX)) {
        shelterR47ConsoleDrawSprite(work->labelX, labelY, (s16)(labelRowIndex + SHELTER_R47_CONSOLE_LABEL_SPRITE_BASE));
    }
}

/// Draws the status reveal cursor and covers the message's unrevealed tail.
///
/// Requires live console work, a valid status 0..9 and its loaded reveal table.
/// `messageY` is a display-centred pixel coordinate. Advances one stop on odd
/// animation frames, then blinks the preceding stop for eight of sixteen frames.
/// Status 3 starts with a terminator and reads the byte preceding its table;
/// the containing storage contract for that empty-message case is unproven.
/// Queues a marker and, before the terminator, one raw textured quad in slot 10.
static void _shelterR47ConsoleDrawStatusReveal(Task* task, s16 messageY)
{
    enum {
        SHELTER_R47_CONSOLE_REVEAL_END        = 255,
        SHELTER_R47_CONSOLE_REVEAL_BLINK_MASK = 15,
        SHELTER_R47_CONSOLE_REVEAL_BLINK_ON   = 8,
        SHELTER_R47_CONSOLE_SPRITE_OT         = 10,
    };
    ShelterR47ConsoleWork* work;
    POLY_FT4*              maskQuad;
    const u8*              revealStop;
    s32                    stopX;

    work       = task->work;
    revealStop = D_shelter_r47_80187374[work->status] + work->revealPos;
    stopX      = *revealStop;
    if (stopX != SHELTER_R47_CONSOLE_REVEAL_END) {
        shelterR47ConsoleDrawSprite(stopX - 0x9D, messageY, SHELTER_R47_CONSOLE_MARKER_SPRITE);
        // Stretch the inset texture over the unrevealed tail of the message.
        maskQuad       = gGpuPrimCursor;
        gGpuPrimCursor = maskQuad + 1;
        setPolyFT4(maskQuad);
        setUVWH(maskQuad, 0x48, 0xB9, 0x2C, 0xE);
        maskQuad->tpage = getTPage(0, GPU_BLEND_AVERAGE, 832, 0);
        maskQuad->clut  = getClut(48, 255);
        setXY4(maskQuad, stopX - 0x96, messageY + 1, 0x69, messageY + 1, stopX - 0x96, messageY + 0xF, 0x69, messageY + 0xF);
        setShadeTex(maskQuad, 1);
        addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_SPRITE_OT], maskQuad);
        if (gDisplayState.animFrame & 1) {
            work->revealPos++;
        }
    } else {
        // The empty status-3 table also takes this preceding-byte path.
        stopX = revealStop[-1];
        if ((u32)(gDisplayState.animFrame & SHELTER_R47_CONSOLE_REVEAL_BLINK_MASK) < SHELTER_R47_CONSOLE_REVEAL_BLINK_ON) {
            shelterR47ConsoleDrawSprite(stopX - 0x9D, messageY, SHELTER_R47_CONSOLE_MARKER_SPRITE);
        }
    }
}

/// Queues the console's 640x240 scrolling backdrop as three texture strips.
///
/// `scrollPixels` is 0..320 pixels left from its initial display-centred origin.
/// Requires loaded textures, the current OT and arena space for three merged
/// draw-mode/sprite packets; the GPU borrows them in slot 12 until completion.
static void _shelterR47ConsoleDrawScrollingBackdrop(s16 scrollPixels)
{
    enum {
        SHELTER_R47_CONSOLE_BACKDROP_OT = 12,
    };
    SpriteDrawModePacket* packet;
    SPRT*                 sprite;

    packet         = gGpuPrimCursor;
    sprite         = &packet->sprite.sprt;
    gGpuPrimCursor = packet + 1;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(1, GPU_BLEND_AVERAGE, 384, 256)));
    sprite->clut  = getClut(0, 256);
    sprite->x0    = -0xA0 - scrollPixels;
    sprite->y0    = -0x78;
    sprite->u0    = 0;
    sprite->v0    = 0;
    sprite->w     = 0x100;
    sprite->h     = 0xF0;
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE;
    addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_BACKDROP_OT], packet);

    packet         = gGpuPrimCursor;
    sprite         = &packet->sprite.sprt;
    gGpuPrimCursor = packet + 1;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(1, GPU_BLEND_AVERAGE, 512, 256)));
    sprite->x0    = 0x60 - scrollPixels;
    sprite->clut  = getClut(0, 256);
    sprite->y0    = -0x78;
    sprite->u0    = 0;
    sprite->v0    = 0;
    sprite->w     = 0x80;
    sprite->h     = 0xF0;
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE;
    addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_BACKDROP_OT], packet);

    packet         = gGpuPrimCursor;
    sprite         = &packet->sprite.sprt;
    gGpuPrimCursor = packet + 1;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(1, GPU_BLEND_AVERAGE, 896, 0)));
    sprite->clut  = getClut(0, 257);
    sprite->x0    = 0xE0 - scrollPixels;
    sprite->y0    = -0x78;
    sprite->u0    = 0;
    sprite->v0    = 0;
    sprite->w     = 0x100;
    sprite->h     = 0xF0;
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE;
    addPrim(&gGpuCurrentOt[SHELTER_R47_CONSOLE_BACKDROP_OT], packet);
}

/// Fades the console to black, saves its switches and releases the session.
///
/// Console state 13; requires live work and the prompt task in spawn argument 2.
/// The fade increases by 16 from 0..255; crossing 255 restores player drawing
/// and control, releases the menu hold and requests both tasks' teardown.
static void _shelterR47ConsoleFadeOutTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_EXIT_FADE_STEP = 16,
    };
    enum { SHELTER_R47_CONSOLE_MENU_UNLOCK_UPDATES = 8 };
    ShelterR47ConsoleWork* work;
    ShelterR47ConsoleWork* switchWork;
    u16                    nextFade;
    u8                     fadeLevel;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    nextFade   = work->fade + SHELTER_R47_CONSOLE_EXIT_FADE_STEP;
    work->fade = nextFade;
    // Commit the switches and release control only once the screen is black.
    if ((s16)nextFade >= SHELTER_R47_CONSOLE_LEVEL_MAX + 1) {
        work->fade = SHELTER_R47_CONSOLE_LEVEL_MAX;
        switchWork = task->work;
        gameFlagSetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED, switchWork->toggles[0]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2, switchWork->toggles[1]);
        gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS, switchWork->toggles[2]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4, switchWork->toggles[3]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, switchWork->toggles[4]);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        Gp_MenuLockDelay = SHELTER_R47_CONSOLE_MENU_UNLOCK_UPDATES;
        D_80114D08       = SHELTER_R47_CONSOLE_INTERACTION_REARM_UPDATES;
        displayReleaseMenuHold();
        gGameSession->eventState   = 0;
        gGameSession->hideHud      = 0;
        gGameSession->cutsceneHold = 0;
        taskKill(task->spawnArg2.pointer);
        taskRequestKill(task, 0);
    }
    fadeLevel = (u8)work->fade;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

static s16 func_shelter_r47_801829B8(Task* task, s16 arg1)
{
    ShelterR47ConsoleWork* state;
    s8                     step;

    state = task->work;
    step  = state->guideStep;
    switch (step) {
        case 1:
            if (arg1 != step) {
                capStartSequenceSlot(0x10, 0, 1);
                return 0;
            }
            state->guideStep = 2;
            return 1;
        case 2:
            if (arg1 != step) {
                capStartSequenceSlot(0x10, 0, 2);
                return 0;
            }
            state->guideStep = 3;
            return 1;
        case 3:
            if (arg1 != step) {
                capStartSequenceSlot(0x10, 0, 3);
                return 0;
            }
            state->guideStep = 4;
            return 1;
    }
    return 0;
}

void shelterR47ConsoleLoadSwitches(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW = 18,
        SHELTER_R47_CONSOLE_SWITCH_2_ON_VIEW  = 36,
    };
    ShelterR47ConsoleWork* work = task->work;

    work->toggles[SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR] = gameFlagGetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED);
    work->toggles[SHELTER_R47_CONSOLE_ROW_SWITCH_2]      = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2);
    // Switch 2 has distinct room images for its off and on states.
    if (!(work->toggles[SHELTER_R47_CONSOLE_ROW_SWITCH_2] & 1)) {
        D_shelter_r47_80186FAC[SHELTER_R47_CONSOLE_ROW_SWITCH_2] = SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW;
    } else {
        D_shelter_r47_80186FAC[SHELTER_R47_CONSOLE_ROW_SWITCH_2] = SHELTER_R47_CONSOLE_SWITCH_2_ON_VIEW;
    }
    work->toggles[SHELTER_R47_CONSOLE_ROW_OBSERVATORY] = gameFlagGetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS);
    work->toggles[SHELTER_R47_CONSOLE_ROW_SWITCH_4]    = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4);
    work->toggles[SHELTER_R47_CONSOLE_ROW_WATCHERS]    = gameFlagGetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED);
}

void func_shelter_r47_80182B18(Task* task)
{
    TaskFuncTable14 states;

    states = D_shelter_r47_8017D6C8;
    states.funcs[task->state](task);
}

s32 shelterR47ConsoleHitTestHotspots(Task* task, ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)
{
    enum {
        SHELTER_R47_CONSOLE_BUTTON_HOTSPOT = 0x101,
    };
    ShelterR47ConsoleWork* work;
    s32                    anyHit;

    work   = task->work;
    anyHit = 0;
    while (hotspots->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((cursorX >= hotspots->x) && ((hotspots->x + hotspots->w) >= cursorX) && (cursorY >= hotspots->y) && ((hotspots->y + hotspots->h) >= cursorY) &&
            ((work->row != SHELTER_R47_CONSOLE_ROW_SWITCH_2) || (hotspots->id != SHELTER_R47_CONSOLE_BUTTON_HOTSPOT))) {
            hotspots->hit = 1;
            anyHit        = 1;
        } else {
            hotspots->hit = 0;
        }
        hotspots++;
    }
    return anyHit;
}

/// Hides and centres the console action cursor before its opening wipe.
///
/// Console state 1; requires the live singleton action prompt and advances to
/// state 2 without allocating or releasing either task.
static void _shelterR47ConsoleResetPromptTask(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

static void func_shelter_r47_80182CA4(Task* task)
{
    ShelterR47ConsoleWork* state;
    s32                    flag;
    s32                    value;

    state = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    if (shelterR47ConsoleClearWipe(task) != 0) {
        if (state->guideStep == 1) {
            capStartSequenceSlot(0xA, 0, 0);
        }
        if (state->guideStep == 0) {
            func_shelter_r47_801832E4(state->status);
        }
        switch (((ShelterR47ConsoleWork*)task->work)->status) {
            case 0:
                flag  = 0x1C6;
                value = 2;
                break;
            case 1:
                flag  = 0x1C6;
                value = 0;
                break;
            case 4:
                flag  = 0x1C4;
                value = 2;
                break;
            case 5:
                flag  = 0x1C4;
                value = 0;
                break;
            default:
                task->state++;
                return;
        }
        gameFlagSetNibble(flag, value);
        task->state++;
    }
}

/// Opens the console hotspot's command menu after hiding and stopping its cursor.
///
/// Console state 4; requires live console work and the first port's prompt.
/// Free-use row selection and switch-button commands offer Examine until
/// their first-use CAP event has run, retaining the hotspot's command kind
/// otherwise. Menu coordinates are display-centred pixels. Advances to
/// state 5 without checking queue acceptance.
static void _shelterR47ConsoleOpenCommandsTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_HOTSPOT_KIND_SHIFT    = 8,
        SHELTER_R47_CONSOLE_HOTSPOT_ROW           = 0,
        SHELTER_R47_CONSOLE_HOTSPOT_SWITCH_BUTTON = 1,
        SHELTER_R47_CONSOLE_GUIDE_FREE_USE        = 0,
        SHELTER_R47_CONSOLE_COMMAND_EXAMINE       = 0,
        SHELTER_R47_CONSOLE_STATE_HANDLE_COMMAND  = 5,
    };
    ShelterR47ConsoleWork* work;
    ActionPrompt*          prompt = D_80114D28;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    // An unplayed first-use event takes precedence over the hotspot's command kind.
    if (work->guideStep == SHELTER_R47_CONSOLE_GUIDE_FREE_USE &&
        ((u16)work->selection >> SHELTER_R47_CONSOLE_HOTSPOT_KIND_SHIFT) == SHELTER_R47_CONSOLE_HOTSPOT_ROW && D_shelter_r47_8018A695 == 0) {
        work->promptKind = SHELTER_R47_CONSOLE_COMMAND_EXAMINE;
    }
    if ((work->selection >> SHELTER_R47_CONSOLE_HOTSPOT_KIND_SHIFT) == SHELTER_R47_CONSOLE_HOTSPOT_SWITCH_BUTTON && D_shelter_r47_8018A694 == 0) {
        work->promptKind = SHELTER_R47_CONSOLE_COMMAND_EXAMINE;
    }
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = SHELTER_R47_CONSOLE_STATE_HANDLE_COMMAND;
}

/// Saves console switches, restores the entry view and ends player use.
///
/// Console state 6; requires live console work and its prompt task in spawn
/// argument 2. Resumes player control/drawing, clears session holds and requests
/// teardown after restoring the saved view; the task system owns the work.
static void _shelterR47ConsoleDismissTask(Task* task)
{
    ShelterR47ConsoleWork* work;

    work       = task->work;
    D_80114D08 = SHELTER_R47_CONSOLE_INTERACTION_REARM_UPDATES;
    _shelterR47ConsoleSaveSwitches(task);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->savedView;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

static void func_shelter_r47_80182F18(Task* task)
{
    s16 step;
    s32 flag;
    s32 value;

    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    if (shelterR47ConsoleClearWipe(task) != 0) {
        func_shelter_r47_801832EC(task);
        step = ((ShelterR47ConsoleWork*)task->work)->status;
        switch (step) {
            case 0:
                flag  = 0x1C6;
                value = 2;
                break;
            case 1:
                flag  = 0x1C6;
                value = 0;
                break;
            case 4:
                flag  = 0x1C4;
                value = 2;
                break;
            case 5:
                flag  = 0x1C4;
                value = 0;
                break;
            default:
                task->state = 3;
                return;
        }
        gameFlagSetNibble(flag, value);
        task->state = 3;
    }
}

/// Wipes over a row change, then selects its view and resets the reveal.
///
/// Console state 7; the previous row remains displayed until the wipe is black.
/// The selection's low byte must be a row 0..4. On completion, fills all wipe
/// components with 255 and advances to state 8, which clears the new view.
static void _shelterR47ConsoleChangeViewTask(Task* task)
{
    ShelterR47ConsoleWork* work;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CHANGING_VIEW);
    // Change the room image only after the circular wipe has hidden it.
    if (shelterR47ConsoleFillWipe(task) != 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_r47_80186FAC[(u8)work->selection];
        work->revealPos                                            = 0;
        _shelterR47ConsoleSetWipe(task, SHELTER_R47_CONSOLE_LEVEL_MAX);
        task->state++;
    }
}

/// Starts the console switch-button flash and restarts the status reveal.
///
/// Console state 9; draws the old state first, then sets a 16-frame flash and
/// advances to state 10, where the selected switch is toggled.
static void _shelterR47ConsoleBeginButtonPressTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_BUTTON_FLASH_FRAMES = 16,
    };
    ShelterR47ConsoleWork* work;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    work->revealPos   = 0;
    work->buttonFlash = SHELTER_R47_CONSOLE_BUTTON_FLASH_FRAMES;
    task->state++;
}

/// Toggles the selected console switch and publishes its access-map state.
///
/// Console state 10; requires a selected row 0..4. Redraw selects the new status
/// before the transfer-door and observatory map marks are updated. Advances to
/// state 11, where input waits for the flash and CAP playback to finish.
static void _shelterR47ConsoleApplyButtonPressTask(Task* task)
{
    enum { SHELTER_R47_CONSOLE_MAP_MARK_HIDDEN  = 0,
           SHELTER_R47_CONSOLE_MAP_MARK_VISIBLE = 2 };
    ShelterR47ConsoleWork* work;

    work = task->work;
    _shelterR47ConsoleToggleSwitch(task, work->row);
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    // Recompute status before updating the two access-related map markers.
    switch (work->status) {
        case SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_OFF:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_R47_1C6, SHELTER_R47_CONSOLE_MAP_MARK_VISIBLE);
            break;
        case SHELTER_R47_CONSOLE_STATUS_TRANSFER_DOOR_ON:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_R47_1C6, SHELTER_R47_CONSOLE_MAP_MARK_HIDDEN);
            break;
        case SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_OFF:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR, SHELTER_R47_CONSOLE_MAP_MARK_VISIBLE);
            break;
        case SHELTER_R47_CONSOLE_STATUS_OBSERVATORY_ON:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR, SHELTER_R47_CONSOLE_MAP_MARK_HIDDEN);
            break;
        case SHELTER_R47_CONSOLE_STATUS_SWITCH_2_OFF:
        case SHELTER_R47_CONSOLE_STATUS_SWITCH_2_ON:
        case SHELTER_R47_CONSOLE_STATUS_SWITCH_4_OFF:
        case SHELTER_R47_CONSOLE_STATUS_SWITCH_4_ON:
        case SHELTER_R47_CONSOLE_STATUS_WATCHERS_OFF:
        case SHELTER_R47_CONSOLE_STATUS_WATCHERS_ON:
            break;
    }
    task->state++;
}

/// Keeps drawing the pressed console button until its flash and CAP playback end.
///
/// Console state 11; requires live console work. The redraw decrements the
/// remaining flash frames; state 3 resumes hotspot input only once both the
/// flash and global CAP playback are idle.
static void _shelterR47ConsoleWaitButtonFlashTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_STATE_IDLE = 3,
    };
    ShelterR47ConsoleWork* work;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    if ((work->buttonFlash == 0) && (capIsBusy() == 0)) {
        task->state = SHELTER_R47_CONSOLE_STATE_IDLE;
    }
}

/// Draws the console once and starts its closing fade from clear.
///
/// Console state 12; requires live console work. Resets the fade after drawing
/// and advances to state 13; switch saving and task teardown happen there.
static void _shelterR47ConsoleBeginFadeOutTask(Task* task)
{
    enum {
        SHELTER_R47_CONSOLE_FADE_CLEAR = 0,
    };
    ShelterR47ConsoleWork* work;

    work = task->work;
    shelterR47ConsoleUpdateAndDraw(task, SHELTER_R47_CONSOLE_LAYOUT_CURRENT);
    work->fade = SHELTER_R47_CONSOLE_FADE_CLEAR;
    task->state++;
}

void shelterR47ResetTerminalFirstUseEvents(void)
{
    D_shelter_r47_8018A694 = false;
    D_shelter_r47_8018A695 = false;
    D_shelter_r47_8018A696 = false;
    D_shelter_r47_8018A697 = false;
}

void shelterR47ConsolePromptTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    states[task->state](task);
}

#include "../../shared/action_prompt_reset.inc.c"

/// Empty callback receiving the console's free-use status; its intended role is unproven.
static void func_shelter_r47_801832E4(s16 status)
{
}

static void func_shelter_r47_801832EC(Task* task)
{
    ShelterR47ConsoleWork* state = task->work;

    switch (state->guideStep) {
        case 0:
            func_shelter_r47_801832E4(state->status);
            break;
        case 2:
            capStartSequenceSlot(0xB, 0, 0);
            break;
        case 3:
            capStartSequenceSlot(0xC, 0, 0);
            break;
        case 4:
            capStartSequenceSlot(0xD, 0, 0);
            break;
    }
}

/// Writes the console's five working switches back to their saved game flags.
///
/// Requires live console work. Values are passed as stored to the nibble setter;
/// this does not toggle them, restore the view or release the work block.
static void _shelterR47ConsoleSaveSwitches(Task* task)
{
    ShelterR47ConsoleWork* work;

    work = task->work;
    gameFlagSetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED, work->toggles[0]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2, work->toggles[1]);
    gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS, work->toggles[2]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4, work->toggles[3]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, work->toggles[4]);
}

/// Flips one working console switch and applies its immediate display effect.
///
/// `row` must be 0..4 in live console work. Switch 2 selects room view 18 or 36
/// and updates its view-table entry; switch 4 changes the backdrop scroll target.
/// Saved game flags are committed separately when console use ends.
static void _shelterR47ConsoleToggleSwitch(Task* task, s16 row)
{
    enum {
        SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW = 18,
        SHELTER_R47_CONSOLE_SWITCH_2_ON_VIEW  = 36,
    };
    ShelterR47ConsoleWork* work;

    work               = task->work;
    work->toggles[row] = (work->toggles[row] + 1) & 1;
    switch (row) {
        case SHELTER_R47_CONSOLE_ROW_TRANSFER_DOOR:
        case SHELTER_R47_CONSOLE_ROW_OBSERVATORY:
        case SHELTER_R47_CONSOLE_ROW_WATCHERS:
            break;
        case SHELTER_R47_CONSOLE_ROW_SWITCH_2:
            if (!(work->toggles[1] & 1)) {
                D_shelter_r47_80186FAC[1]                                  = SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = SHELTER_R47_CONSOLE_SWITCH_2_OFF_VIEW;
            } else {
                D_shelter_r47_80186FAC[1]                                  = SHELTER_R47_CONSOLE_SWITCH_2_ON_VIEW;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = SHELTER_R47_CONSOLE_SWITCH_2_ON_VIEW;
            }
            break;
        case SHELTER_R47_CONSOLE_ROW_SWITCH_4:
            if (!(work->toggles[3] & 1)) {
                work->backdropToggle = 0;
            } else {
                work->backdropToggle = 1;
            }
            break;
    }
}

/// `AREA_SAVED_MAP_MARK` of the area's saved state, as 0 or 1; 0 when the stage
/// has no table or the area no saved state. The counterpart of `areaIsSavedPoseRestoreEnabled`.
static inline s32 _shelterR47GetAreaFlag4(GameLocationKey* key)
{
    AreaRecord*     rec;
    AreaSavedState* areaState;
    s16             val;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        areaState = rec[key->area].savedState;
        if (areaState != NULL) {
            val = areaState->spawnFlags & AREA_SAVED_MAP_MARK;
            return val != 0;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

/// Whether area `area` of stage `stage` (view 2, room 1) has
/// `AREA_SAVED_MAP_MARK` set and `AREA_SPAWN_RESTORE_SAVED_POSES` clear. This is
/// the condition under which `Gp_RebuildAreaIdBits` sets the area's bit in
/// `Gp_AreaIdBits`.
static inline s16 _shelterR47IsAreaMarked(s32 stage, s32 area)
{
    GameLocationKey key;

    key.stage = stage;
    key.room  = 1;
    key.view  = 2;
    key.area  = area;
    if (_shelterR47GetAreaFlag4(&key) != 1 || areaIsSavedPoseRestoreEnabled(&key) == 1) {
        return 0;
    }
    return 1;
}

/// Draws the map overlay of the room's map terminal, brightening each
/// marker by 0x30 over the last. While `page` is not Neo Ark it draws one marker
/// per entry of that page's marker table whose saved area state has
/// `AREA_SAVED_MAP_MARK` set and `AREA_SPAWN_RESTORE_SAVED_POSES` clear, after a
/// fixed marker when `page` is B1, `openMode` is not the timed viewing and
/// collected bit 0x12D is set. When `page` is Neo Ark it first moves `openMode`
/// from the tour to tour-done and starts cap slot 0x13, then draws
/// the same markers if `GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED` is 1, and otherwise the
/// `mapWidth` x `mapHeight` map quad.
static void func_shelter_r47_80183484(Task* task)
{
    ShelterR47MapTerminalWork* state;
    _ShelterR47MapMark*        mark;
    POLY_FT4*                  p;
    u8                         shade;

    state = (ShelterR47MapTerminalWork*)task->work;
    shade = (u8)state->openFrames * 4;
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 1) {
        mark = D_shelter_r47_801875D8[state->page];
    } else {
        mark = D_shelter_r47_801875C4[state->page];
    }
    if (state->page != SHELTER_R47_MAP_PAGE_NEO_ARK) {
        if (state->openMode != SHELTER_R47_MAP_MODE_TIMED && state->page == SHELTER_R47_MAP_PAGE_B1 && inventoryHasCollectedBit(0x12D) != 0) {
            shade         += 0x30;
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setPolyFT4(p);
            setUV4(p, 0x58, 0x20, 0x60, 0x20, 0x58, 0x28, 0x60, 0x28);
            p->tpage = 0x2F;
            p->clut  = 0x3FC7;
            setRGB0(p, shade, shade, shade);
            setSemiTrans(p, 1);
            setXY4(p, 0x78, -0x4B, 0x80, -0x4B, 0x78, -0x43, 0x80, -0x43);
            addPrim(&gGpuCurrentOt[10], p);
        }
        while (mark->stage != SHELTER_R47_MAP_MARK_END) {
            if (_shelterR47IsAreaMarked(mark->stage, mark->area)) {
                p              = gGpuPrimCursor;
                shade         += 0x30;
                gGpuPrimCursor = p + 1;
                setPolyFT4(p);
                setUV4(p, 0x50, 0x20, 0x58, 0x20, 0x50, 0x28, 0x58, 0x28);
                setRGB0(p, shade, shade, shade);
                p->tpage = 0x2F;
                p->clut  = 0x3FC6;
                setSemiTrans(p, 1);
                setXY4(p, mark->x - 4, mark->y - 4, mark->x + 4, mark->y - 4, mark->x - 4, mark->y + 4,
                       mark->x + 4, mark->y + 4);
                addPrim(&gGpuCurrentOt[10], p);
            }
            mark++;
        }
    } else {
        if (state->openMode == SHELTER_R47_MAP_MODE_TOUR) {
            state->openMode = SHELTER_R47_MAP_MODE_TOUR_DONE;
            capStartSequenceSlot(0x13, 0, 0);
        }
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 1) {
            while (mark->stage != SHELTER_R47_MAP_MARK_END) {
                if (_shelterR47IsAreaMarked(mark->stage, mark->area)) {
                    p              = gGpuPrimCursor;
                    shade         += 0x30;
                    gGpuPrimCursor = p + 1;
                    setPolyFT4(p);
                    setUV4(p, 0x50, 0x20, 0x58, 0x20, 0x50, 0x28, 0x58, 0x28);
                    setRGB0(p, shade, shade, shade);
                    p->tpage = 0x2F;
                    p->clut  = 0x3FC6;
                    setSemiTrans(p, 1);
                    setXY4(p, mark->x - 4, mark->y - 4, mark->x + 4, mark->y - 4, mark->x - 4, mark->y + 4,
                           mark->x + 4, mark->y + 4);
                    addPrim(&gGpuCurrentOt[10], p);
                }
                mark++;
            }
        } else {
            if (shade == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_R47_MAP_TERMINAL_LOOP, 0, 0);
            }
            p              = gGpuPrimCursor;
            gGpuPrimCursor = p + 1;
            setPolyFT4(p);
            setUV4(p, 0, 0, 0xE8, 0, 0, 0xCE, 0xE8, 0xCE);
            p->tpage = 0x36;
            p->clut  = 0x4000;
            setRGB0(p, shade, shade, shade);
            setSemiTrans(p, 1);
            setXY4(p, -0x4D, -0x67, state->mapWidth - 0x4D, -0x67, -0x4D, state->mapHeight - 0x67,
                   state->mapWidth - 0x4D, state->mapHeight - 0x67);
            addPrim(&gGpuCurrentOt[11], p);
        }
    }
}

void func_shelter_r47_80183B84(Task* task)
{
    ShelterR47MapTerminalWork* state;
    POLY_FT4*                  p;

    state             = (ShelterR47MapTerminalWork*)task->work;
    state->mapWidth  += (state->mapTargetWidth - state->mapWidth) >> 2;
    state->mapHeight += (state->mapTargetHeight - state->mapHeight) >> 2;
    if (state->mapWidth >= SHELTER_R47_MAP_WIDTH_SETTLED) {
        state->mapWidth  = SHELTER_R47_MAP_WIDTH;
        state->mapHeight = SHELTER_R47_MAP_HEIGHT;
        func_shelter_r47_80183484(task);
        state->holdPrompt = 0;
        state->openFrames++;
    } else {
        state->openFrames = 0;
        state->holdPrompt = 1;
    }

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    setUV4(p, 0, 0, 0xE8, 0, 0, 0xCE, 0xE8, 0xCE);
    p->tpage = 0x2D;
    p->clut  = 0x3FC0;
    p->code |= 3;
    setXY4(p, -0x4D, -0x67, state->mapWidth - 0x4D, -0x67, -0x4D, state->mapHeight - 0x67,
           state->mapWidth - 0x4D, state->mapHeight - 0x67);
    addPrim(&gGpuCurrentOt[12], p);

    p                   = gGpuPrimCursor;
    state->panelWidth  += (state->panelTargetWidth - state->panelWidth) >> 2;
    state->panelHeight += (state->panelTargetHeight - state->panelHeight) >> 2;
    gGpuPrimCursor      = p + 1;
    setPolyFT4(p);
    setUV4(p, 0, 0, 0x50, 0, 0, 0x60, 0x50, 0x60);
    p->tpage = 0x2E;
    p->clut  = 0x3FC1;
    p->code |= 3;
    setXY4(p, -0x9C, -0x5B, state->panelWidth - 0x9C, -0x5B, -0x9C, state->panelHeight - 0x5B,
           state->panelWidth - 0x9C, state->panelHeight - 0x5B);
    addPrim(&gGpuCurrentOt[11], p);
}

void shelterR47MapTerminalDrawPreviousButton(void)
{
    SpriteDrawModePacket* packet;
    SPRT*                 sprite;

    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    sprite         = &packet->sprite.sprt;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD, 960, 0)));
    sprite->clut  = getClut(64, 255);
    sprite->x0    = -0x96;
    sprite->y0    = 0x3F;
    sprite->u0    = 0x50;
    sprite->w     = 0x38;
    sprite->v0    = 0;
    sprite->h     = 0x10;
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE | SPRITE_SOURCE_SEMI_TRANSPARENT;
    addPrim(&gGpuCurrentOt[SHELTER_R47_MAP_TERMINAL_LABEL_OT], packet);
}

void shelterR47MapTerminalDrawNextButton(void)
{
    SpriteDrawModePacket* packet;
    SPRT*                 sprite;

    packet         = gGpuPrimCursor;
    gGpuPrimCursor = packet + 1;
    sprite         = &packet->sprite.sprt;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD, 960, 0)));
    sprite->clut  = getClut(80, 255);
    sprite->x0    = -0x90;
    sprite->y0    = 0x50;
    sprite->u0    = 0x50;
    sprite->v0    = 0x10;
    sprite->w     = 0x38;
    sprite->h     = 0x10;
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE | SPRITE_SOURCE_SEMI_TRANSPARENT;
    addPrim(&gGpuCurrentOt[SHELTER_R47_MAP_TERMINAL_LABEL_OT], packet);
}

void shelterR47MapTerminalDrawPageTitle(Task* task, s16 page)
{
    SpriteDrawModePacket*      packet;
    SPRT*                      sprite;
    ShelterR47MapTerminalWork* work;

    packet         = gGpuPrimCursor;
    work           = task->work;
    sprite         = &packet->sprite.sprt;
    gGpuPrimCursor = packet + 1;
    work->labelX  += (work->labelTargetX - work->labelX) >> 2;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD, 960, 0)));
    sprite->clut  = getClut(32, 255);
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE | SPRITE_SOURCE_SEMI_TRANSPARENT;
    sprite->x0    = work->labelX;
    sprite->y0    = -0x67;
    sprite->u0    = 0;
    sprite->v0    = D_shelter_r47_801875EC[page];
    sprite->w     = 0x50;
    sprite->h     = 0xA;
    addPrim(&gGpuCurrentOt[SHELTER_R47_MAP_TERMINAL_LABEL_OT], packet);
}

void shelterR47MapTerminalDrawPageCaptions(Task* task, s16 page)
{
    SpriteDrawModePacket*      packet;
    SPRT*                      sprite;
    ShelterR47MapTerminalWork* work;

    packet         = gGpuPrimCursor;
    work           = task->work;
    sprite         = &packet->sprite.sprt;
    gGpuPrimCursor = packet + 1;
    _shelterR47InitDrawModeSprite(packet, (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD, 960, 0)));
    sprite->clut  = getClut(48, 255);
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE | SPRITE_SOURCE_SEMI_TRANSPARENT;
    sprite->x0    = work->labelX;
    sprite->y0    = 0x35;
    sprite->u0    = 0;
    sprite->v0    = D_shelter_r47_801875F8[page][0] + 0x38;
    sprite->w     = 0x50;
    sprite->h     = 8;
    addPrim(&gGpuCurrentOt[SHELTER_R47_MAP_TERMINAL_LABEL_OT], packet);

    packet                   = gGpuPrimCursor;
    sprite                   = &packet->sprite.sprt;
    gGpuPrimCursor           = packet + 1;
    packet->drawMode.code[0] = (SHELTER_R47_DRAW_MODE_COMMAND | getTPage(0, GPU_BLEND_ADD, 960, 0));
    setlen(&packet->drawMode, ARRAY_SIZE(packet->drawMode.code));
    setSprt(&packet->sprite.sprt);
    MargePrim(packet, sprite);
    sprite->clut  = getClut(48, 255);
    sprite->code |= SPRITE_SOURCE_RAW_TEXTURE | SPRITE_SOURCE_SEMI_TRANSPARENT;
    sprite->x0    = work->labelX;
    sprite->y0    = 0x60;
    sprite->u0    = 0;
    sprite->v0    = D_shelter_r47_801875F8[page][1] + 0x38;
    sprite->w     = 0x50;
    sprite->h     = 8;
    addPrim(&gGpuCurrentOt[SHELTER_R47_MAP_TERMINAL_LABEL_OT], packet);
}
