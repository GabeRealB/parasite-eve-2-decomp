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

static void _actionPromptResetDefault(Task* task);
static void func_shelter_r47_801816CC(Task* task);
static void func_shelter_r47_80181F14(Task* task, s16 y);
static void func_shelter_r47_801820C0(s16 arg0);
static void func_shelter_r47_80182348(Task* task);
static s16  func_shelter_r47_801829B8(Task* task, s16 arg1);
static void func_shelter_r47_80182C78(Task* task);
static void func_shelter_r47_80182CA4(Task* task);
static void func_shelter_r47_80182DAC(Task* task);
static void func_shelter_r47_80182E78(Task* task);
static void func_shelter_r47_80182F18(Task* task);
static void func_shelter_r47_80182FDC(Task* task);
static void func_shelter_r47_80183068(Task* task);
static void func_shelter_r47_801830B8(Task* task);
static void func_shelter_r47_80183170(Task* task);
static void func_shelter_r47_801831C8(Task* task);
static void func_shelter_r47_801832E4(s16 step);
static void func_shelter_r47_801832EC(Task* task);
static void func_shelter_r47_8018337C(Task* task);
static void func_shelter_r47_801833DC(Task* task, s16 arg1);
static void func_shelter_r47_80183484(Task* task);

/// State handlers of the room's first cap script, run by
/// `func_shelter_r47_80182B18`.
static const TaskFuncTable14 D_shelter_r47_8017D6C8 = {
    {
        func_shelter_r47_8018138C,
        func_shelter_r47_80182C78,
        func_shelter_r47_80182CA4,
        func_shelter_r47_80181568,
        func_shelter_r47_80182DAC,
        func_shelter_r47_801816CC,
        func_shelter_r47_80182E78,
        func_shelter_r47_80182FDC,
        func_shelter_r47_80182F18,
        func_shelter_r47_80183068,
        func_shelter_r47_801830B8,
        func_shelter_r47_80183170,
        func_shelter_r47_801831C8,
        func_shelter_r47_80182348,
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
    func_shelter_r47_80181914(task, 0);
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

/// Per-frame draw of the cap script's selection screen. While `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` is
/// 0x14 it scrolls the background by `backdropScroll`. Every positioned sprite is
/// eased a quarter of the way toward its target each frame. `arg1` picks the
/// layout: the entry drawn is `row` when it is 0 and `previousRow` otherwise,
/// that entry's toggle selects `status`, and the five rows
/// at `rowX`/`rowY` either all settle at one column or fan out, with the
/// selected row marked.
void func_shelter_r47_80181914(Task* task, s16 arg1)
{
    ShelterR47ConsoleWork* work;
    s32                    i;
    s16                    id;
    s16                    nx;
    s32                    x;
    s32                    ny;
    s16                    y;
    s8                     c;
    s16                    sel;

    work = task->work;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 0x14) {
        if (work->backdropToggle & 1) {
            work->backdropScroll--;
            if (work->backdropScroll < 0) {
                work->backdropScroll = 0;
            }
        } else {
            work->backdropScroll++;
            if (work->backdropScroll > 0x140) {
                work->backdropScroll = 0x140;
            }
        }
        func_shelter_r47_801820C0(work->backdropScroll);
    }
    if (work->buttonFlash > 0) {
        work->buttonFlash--;
    }
    work->headerX += (-0x98 - work->headerX) >> 2;
    shelterR47ConsoleDrawSprite(work->headerX, work->headerY, 0);
    id = 1;
    if (arg1 == 0) {
        work->buttonY += (0x48 - work->buttonY) >> 2;
        if (work->buttonFlash == 0) {
            shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, id);
        } else {
            shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, 2);
        }
        work->messageY += (0x58 - work->messageY) >> 2;
        func_shelter_r47_80181F14(task, work->messageY);
        switch (work->row) {
            case 0:
                if (work->toggles[0] == 0) {
                    SHELTER_R47_DRAW_STEP(3, 0);
                } else {
                    SHELTER_R47_DRAW_STEP(4, 1);
                }
                break;
            case 1:
                if (work->toggles[1] == 0) {
                    SHELTER_R47_DRAW_STEP(5, 2);
                } else {
                    SHELTER_R47_DRAW_STEP(6, 3);
                }
                break;
            case 2:
                if (work->toggles[2] == 0) {
                    SHELTER_R47_DRAW_STEP(7, 4);
                } else {
                    SHELTER_R47_DRAW_STEP(8, 5);
                }
                break;
            case 3:
                if (work->toggles[3] == 0) {
                    SHELTER_R47_DRAW_STEP(9, 6);
                } else {
                    SHELTER_R47_DRAW_STEP(10, 7);
                }
                break;
            case 4:
                if (work->toggles[4] == 0) {
                    SHELTER_R47_DRAW_STEP(11, 8);
                } else {
                    SHELTER_R47_DRAW_STEP(12, 9);
                }
                break;
        }
    } else {
        work->buttonY += (0x80 - work->buttonY) >> 2;
        shelterR47ConsoleDrawSprite(work->buttonX, work->buttonY, id);
        work->messageY += (0x90 - work->messageY) >> 2;
        func_shelter_r47_80181F14(task, work->messageY);
        switch (work->previousRow) {
            case 0:
                if (work->toggles[0] == 0) {
                    SHELTER_R47_DRAW_STEP(3, 0);
                } else {
                    SHELTER_R47_DRAW_STEP(4, 1);
                }
                break;
            case 1:
                if (work->toggles[1] == 0) {
                    SHELTER_R47_DRAW_STEP(5, 2);
                } else {
                    SHELTER_R47_DRAW_STEP(6, 3);
                }
                break;
            case 2:
                if (work->toggles[2] == 0) {
                    SHELTER_R47_DRAW_STEP(7, 4);
                } else {
                    SHELTER_R47_DRAW_STEP(8, 5);
                }
                break;
            case 3:
                if (work->toggles[3] == 0) {
                    SHELTER_R47_DRAW_STEP(9, 6);
                } else {
                    SHELTER_R47_DRAW_STEP(10, 7);
                }
                break;
            case 4:
                if (work->toggles[4] == 0) {
                    SHELTER_R47_DRAW_STEP(11, 8);
                } else {
                    SHELTER_R47_DRAW_STEP(12, 9);
                }
                break;
        }
    }
    if (arg1 == 0) {
        for (i = 0; i < 5; i++) {
            nx            = work->rowX[i] + ((0x78 - work->rowX[i]) >> 2);
            work->rowX[i] = nx;
            if (work->row == i) {
                ny = work->rowY[i];
                shelterR47ConsoleDrawSprite((s16)(nx + 0x18), ny, 0x14);
                shelterR47ConsoleDrawSprite(nx, ny, 0x12);
            } else {
                shelterR47ConsoleDrawSprite(nx, work->rowY[i], 0x12);
            }
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (work->row == i) {
                nx            = work->rowX[i] + ((0x78 - work->rowX[i]) >> 2);
                ny            = work->rowY[i];
                work->rowX[i] = nx;
                shelterR47ConsoleDrawSprite((s16)(nx + 0x18), ny, 0x14);
                shelterR47ConsoleDrawSprite(nx, ny, 0x13);
            } else {
                switch (i) {
                    case 0:
                        work->rowX[i] += (0xAA - work->rowX[i]) >> 2;
                        break;
                    case 1:
                        work->rowX[i] += (0xBE - work->rowX[i]) >> 2;
                        break;
                    case 2:
                        work->rowX[i] += (0xD2 - work->rowX[i]) >> 2;
                        break;
                    case 3:
                        work->rowX[i] += (0xE6 - work->rowX[i]) >> 2;
                        break;
                    case 4:
                        work->rowX[i] += (0xFA - work->rowX[i]) >> 2;
                        break;
                }
                shelterR47ConsoleDrawSprite(work->rowX[i], work->rowY[i], 0x12);
            }
        }
    }
    if (arg1 == 0) {
        c = work->row;
        x = work->labelX;
        y = work->labelY;
    } else {
        c = work->previousRow;
        x = work->labelX;
        y = work->labelY;
    }
    work->labelX += (0x7E - x) >> 2;
    sel           = c;
    if ((u16)sel < 5) {
        shelterR47ConsoleDrawSprite(work->labelX, y, (s16)(sel + 0xD));
    }
}

/// Draws the current reveal stop of status message `status` at row `y` through
/// `shelterR47ConsoleDrawSprite`, with a textured quad whose left edge follows
/// the stop's x, advancing `revealPos` on odd animation frames. At the
/// terminator it redraws the previous stop for eight frames out of every
/// sixteen instead.
static void func_shelter_r47_80181F14(Task* task, s16 y)
{
    ShelterR47ConsoleWork* work;
    POLY_FT4*              poly;
    u8*                    p;
    s32                    c;

    work = task->work;
    p    = D_shelter_r47_80187374[work->status] + work->revealPos;
    c    = *p;
    if (c != 0xFF) {
        shelterR47ConsoleDrawSprite(c - 0x9D, y, 0x14);
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setPolyFT4(poly);
        setUVWH(poly, 0x48, 0xB9, 0x2C, 0xE);
        poly->tpage = 0xD;
        poly->clut  = 0x3FC3;
        setXY4(poly, c - 0x96, y + 1, 0x69, y + 1, c - 0x96, y + 0xF, 0x69, y + 0xF);
        poly->code |= 1;
        addPrim(&gGpuCurrentOt[10], poly);
        if (gDisplayState.animFrame & 1) {
            work->revealPos++;
        }
    } else {
        c = p[-1];
        if ((u32)(gDisplayState.animFrame & 0xF) < 8) {
            shelterR47ConsoleDrawSprite(c - 0x9D, y, 0x14);
        }
    }
}

static void func_shelter_r47_801820C0(s16 arg0)
{
    SpriteDrawModePacket* p;
    SPRT*                 sprt;

    p              = gGpuPrimCursor;
    sprt           = &p->sprite.sprt;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE1000096;
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x4000;
    sprt->x0    = -0xA0 - arg0;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x100;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);

    p              = gGpuPrimCursor;
    sprt           = &p->sprite.sprt;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE1000098;
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->x0    = 0x60 - arg0;
    sprt->clut  = 0x4000;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x80;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);

    p              = gGpuPrimCursor;
    sprt           = &p->sprite.sprt;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE100008E;
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x4040;
    sprt->x0    = 0xE0 - arg0;
    sprt->y0    = -0x78;
    sprt->u0    = 0;
    sprt->v0    = 0;
    sprt->w     = 0x100;
    sprt->h     = 0xF0;
    sprt->code |= 1;
    addPrim(&gGpuCurrentOt[12], p);
}

static void func_shelter_r47_80182348(Task* task)
{
    ShelterR47ConsoleWork* state;
    ShelterR47ConsoleWork* done;
    u16                    fade;
    u8                     level;

    state = task->work;
    func_shelter_r47_80181914(task, 0);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        done        = task->work;
        gameFlagSetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED, done->toggles[0]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2, done->toggles[1]);
        gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS, done->toggles[2]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4, done->toggles[3]);
        gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, done->toggles[4]);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        Gp_MenuLockDelay = 8;
        D_80114D08       = 0xA;
        displayReleaseMenuHold();
        gGameSession->eventState   = 0;
        gGameSession->hideHud      = 0;
        gGameSession->cutsceneHold = 0;
        taskKill(task->spawnArg2.pointer);
        taskRequestKill(task, 0);
    }
    level = (u8)state->fade;
    fadeDrawOverlay(level, level, level, GPU_BLEND_SUBTRACT);
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

/// Loads the script's working copies of game flags 0xAC, 0xD5, 0xAE, 0xD6 and
/// 0xD2, and sets `D_shelter_r47_80186FAC[1]` from the low bit of flag 0xD5.
void func_shelter_r47_80182AA0(Task* task)
{
    ShelterR47ConsoleWork* state = task->work;

    state->toggles[0] = gameFlagGetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED);
    state->toggles[1] = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2);
    if (!(state->toggles[1] & 1)) {
        D_shelter_r47_80186FAC[1] = 0x12;
    } else {
        D_shelter_r47_80186FAC[1] = 0x24;
    }
    state->toggles[2] = gameFlagGetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS);
    state->toggles[3] = gameFlagGetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4);
    state->toggles[4] = gameFlagGetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED);
}

void func_shelter_r47_80182B18(Task* task)
{
    TaskFuncTable14 states;

    states = D_shelter_r47_8017D6C8;
    states.funcs[task->state](task);
}

/// Hit-tests the point (`x`, `y`) against every entry of a hotspot table up to
/// its -1 terminator, raising `hit` on each entry whose rectangle contains the
/// point (edges inclusive) and clearing it on the rest. Entry 0x101 is never
/// raised while the task's `row` is 1. Returns 1 if any entry was raised.
s32 func_shelter_r47_80182B9C(Task* task, ActionPromptHotspot* table, s16 x, s16 y)
{
    ShelterR47ConsoleWork* work;
    s32                    hit;

    work = task->work;
    hit  = 0;
    while (table->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y) &&
            ((work->row != 1) || (table->id != 0x101))) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Stops the action prompt, hides its cursor, clears its screen position, and
/// steps the caller's script on one state.
static void func_shelter_r47_80182C78(Task* task)
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
    func_shelter_r47_80181914(task, 0);
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

/// Clears the action prompt's mode and target, drops the prompt kind to 0 when
/// its gating flags are clear, shows the prompt, and moves the script to state 5.
static void func_shelter_r47_80182DAC(Task* task)
{
    ShelterR47ConsoleWork* state;
    ActionPrompt*          prompt = D_80114D28;

    state = task->work;
    func_shelter_r47_80181914(task, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (state->guideStep == 0 && ((u16)state->selection >> 8) == 0 && D_shelter_r47_8018A695 == 0) {
        state->promptKind = 0;
    }
    if ((state->selection >> 8) == 1 && D_shelter_r47_8018A694 == 0) {
        state->promptKind = 0;
    }
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, state->promptKind);
    task->state = 5;
}

static void func_shelter_r47_80182E78(Task* task)
{
    ShelterR47ConsoleWork* state;

    state      = task->work;
    D_80114D08 = 0xA;
    func_shelter_r47_8018337C(task);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = state->savedView;
    /* Keeps the `spawnArg2` load below the `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store, so that it
       does not fill `taskKill`'s delay slot. */
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

static void func_shelter_r47_80182F18(Task* task)
{
    s16 step;
    s32 flag;
    s32 value;

    func_shelter_r47_80181914(task, 0);
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

static void func_shelter_r47_80182FDC(Task* task)
{
    ShelterR47ConsoleWork* state;
    ShelterR47ConsoleWork* work;

    state = task->work;
    func_shelter_r47_80181914(task, 1);
    if (shelterR47ConsoleFillWipe(task) != 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_r47_80186FAC[(u8)state->selection];
        state->revealPos                                           = 0;
        work                                                       = task->work;
        work->wipeRed                                              = 0xFF;
        work->wipeGreen                                            = 0xFF;
        work->wipeBlue                                             = 0xFF;
        work->wipeGrey                                             = 0xFF;
        task->state++;
    }
}

static void func_shelter_r47_80183068(Task* task)
{
    ShelterR47ConsoleWork* state;

    state = task->work;
    func_shelter_r47_80181914(task, 0);
    state->revealPos   = 0;
    state->buttonFlash = 0x10;
    task->state++;
}

static void func_shelter_r47_801830B8(Task* task)
{
    ShelterR47ConsoleWork* state;

    state = task->work;
    func_shelter_r47_801833DC(task, state->row);
    func_shelter_r47_80181914(task, 0);
    switch (state->status) {
        case 0:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_R47_1C6, 2);
            break;
        case 1:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_R47_1C6, 0);
            break;
        case 4:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR, 2);
            break;
        case 5:
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR, 0);
            break;
        case 2:
        case 3:
        case 6:
        case 7:
        case 8:
        case 9:
            break;
    }
    task->state++;
}

static void func_shelter_r47_80183170(Task* task)
{
    ShelterR47ConsoleWork* state;

    state = task->work;
    func_shelter_r47_80181914(task, 0);
    if ((state->buttonFlash == 0) && (capIsBusy() == 0)) {
        task->state = 3;
    }
}

static void func_shelter_r47_801831C8(Task* task)
{
    ShelterR47ConsoleWork* state;

    state = task->work;
    func_shelter_r47_80181914(task, 0);
    state->fade = 0;
    task->state++;
}

void func_shelter_r47_80183210(void)
{
    D_shelter_r47_8018A694 = 0;
    D_shelter_r47_8018A695 = 0;
    D_shelter_r47_8018A696 = 0;
    D_shelter_r47_8018A697 = 0;
}

/// Two-state dispatcher of the action prompt, with its handler table built on
/// the stack: state 0 runs `_actionPromptResetDefault` and state 1 runs
/// `_actionPromptMoveCursorsDefault`.
void func_shelter_r47_80183234(Task* task)
{
    TaskFunc funcs[2] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    funcs[task->state](task);
}

#include "../../shared/action_prompt_reset.inc.c"

static void func_shelter_r47_801832E4(s16 step)
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

static void func_shelter_r47_8018337C(Task* task)
{
    ShelterR47ConsoleWork* state;

    state = task->work;
    gameFlagSetNibble(GAME_FLAG_B1_TRANSFER_TUNNEL_DOOR_UNLOCKED, state->toggles[0]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_2, state->toggles[1]);
    gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_OBSERVATORY_ACCESS, state->toggles[2]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_R47_CONSOLE_SWITCH_4, state->toggles[3]);
    gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, state->toggles[4]);
}

/// Flips toggle `arg1`. Toggle 1 also publishes the area view
/// (0x12 or 0x24), and toggle 3 is mirrored into `backdropToggle`.
static void func_shelter_r47_801833DC(Task* task, s16 arg1)
{
    ShelterR47ConsoleWork* state;

    state                = task->work;
    state->toggles[arg1] = (state->toggles[arg1] + 1) & 1;
    switch (arg1) {
        case 0:
        case 2:
        case 4:
            break;
        case 1:
            if (!(state->toggles[1] & 1)) {
                D_shelter_r47_80186FAC[1]                                  = 0x12;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x12;
            } else {
                D_shelter_r47_80186FAC[1]                                  = 0x24;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x24;
            }
            break;
        case 3:
            if (!(state->toggles[3] & 1)) {
                state->backdropToggle = 0;
            } else {
                state->backdropToggle = 1;
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

void func_shelter_r47_80183E24(void)
{
    SpriteDrawModePacket* p;
    SPRT*                 sprt;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE100002F;
    setcode(&p->sprite.sprt, 0x64);
    sprt = &p->sprite.sprt;
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC4;
    sprt->x0    = -0x96;
    sprt->y0    = 0x3F;
    sprt->u0    = 0x50;
    sprt->w     = 0x38;
    sprt->v0    = 0;
    sprt->h     = 0x10;
    sprt->code |= 3;
    addPrim(&gGpuCurrentOt[11], p);
}

void func_shelter_r47_80183F0C(void)
{
    SpriteDrawModePacket* p;
    SPRT*                 sprt;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE100002F;
    setcode(&p->sprite.sprt, 0x64);
    sprt = &p->sprite.sprt;
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC5;
    sprt->x0    = -0x90;
    sprt->y0    = 0x50;
    sprt->u0    = 0x50;
    sprt->v0    = 0x10;
    sprt->w     = 0x38;
    sprt->h     = 0x10;
    sprt->code |= 3;
    addPrim(&gGpuCurrentOt[11], p);
}

void func_shelter_r47_80183FF4(Task* task, s16 arg1)
{
    SpriteDrawModePacket*      p;
    SPRT*                      sprt;
    ShelterR47MapTerminalWork* state;

    p              = gGpuPrimCursor;
    state          = (ShelterR47MapTerminalWork*)task->work;
    sprt           = &p->sprite.sprt;
    gGpuPrimCursor = p + 1;
    state->labelX += (state->labelTargetX - state->labelX) >> 2;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE100002F;
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC2;
    sprt->code |= 3;
    sprt->x0    = state->labelX;
    sprt->y0    = -0x67;
    sprt->u0    = 0;
    sprt->v0    = D_shelter_r47_801875EC[arg1];
    sprt->w     = 0x50;
    sprt->h     = 0xA;
    addPrim(&gGpuCurrentOt[11], p);
}

void func_shelter_r47_80184124(Task* task, s16 arg1)
{
    SpriteDrawModePacket*      p;
    SPRT*                      sprt;
    ShelterR47MapTerminalWork* state;

    p              = gGpuPrimCursor;
    state          = (ShelterR47MapTerminalWork*)task->work;
    sprt           = &p->sprite.sprt;
    gGpuPrimCursor = p + 1;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    p->drawMode.code[0] = 0xE100002F;
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC3;
    sprt->code |= 3;
    sprt->x0    = state->labelX;
    sprt->y0    = 0x35;
    sprt->u0    = 0;
    sprt->v0    = D_shelter_r47_801875F8[arg1][0] + 0x38;
    sprt->w     = 0x50;
    sprt->h     = 8;
    addPrim(&gGpuCurrentOt[11], p);

    p                   = gGpuPrimCursor;
    sprt                = &p->sprite.sprt;
    gGpuPrimCursor      = p + 1;
    p->drawMode.code[0] = 0xE100002F;
    setlen(&p->drawMode, 1);
    setlen(&p->sprite.sprt, 4);
    setcode(&p->sprite.sprt, 0x64);
    MargePrim(p, sprt);
    sprt->clut  = 0x3FC3;
    sprt->code |= 3;
    sprt->x0    = state->labelX;
    sprt->y0    = 0x60;
    sprt->u0    = 0;
    sprt->v0    = D_shelter_r47_801875F8[arg1][1] + 0x38;
    sprt->w     = 0x50;
    sprt->h     = 8;
    addPrim(&gGpuCurrentOt[11], p);
}
