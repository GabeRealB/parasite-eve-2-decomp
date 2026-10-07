#include "mapui/map_akropolis.h"

#include <psyq/sys/types.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/map.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag_ids.h"
#include "main/gfx_types.h"
#include "main/pad.h"
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

#include "mappic/mappic.h"

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_cafeteria.h"

#include "rooms/acropolis_east_elevator_hall.h"

#include "rooms/acropolis_fire_escape.h"

#include "rooms/acropolis_forked_road.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_hallway.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/acropolis_observatory.h"

#include "rooms/acropolis_patio.h"

#include "rooms/acropolis_plaza.h"

#include "rooms/acropolis_promenade.h"

#include "rooms/acropolis_roof_garden.h"

#include "rooms/acropolis_sanctuary.h"

#include "rooms/acropolis_security_room.h"

#include "rooms/acropolis_square.h"

#include "rooms/acropolis_west_elevator_hall.h"

#include "rooms/mist_parking.h"

#include "rooms/mist_r18.h"

#include "rooms/mist_r21.h"

#include "rooms/mist_shooting_gallery.h"

/* The Akropolis stage's map UI overlay (stage 1, Akropolis and MIST): its
 * stream setup, the key-item panel, and the per-stage tables gameplay and main
 * index by stage, most of which point into the stage's room packages or at the
 * map pictures' marker models.
 */

static void func_map_akropolis_80179C50(UiList* arg0, UiObject* arg1);
static void func_map_akropolis_80179D78(Task* task);
static void func_map_akropolis_80179E8C(Task* task);

static const char      D_map_akropolis_8017997C[12];
static s32             D_map_akropolis_8017A9A8;
static s32             D_map_akropolis_8017A9AC[4];
static UiList          D_map_akropolis_8017A9C0;
static UiObjectDesc    D_map_akropolis_8017A9E4;
static TaskDesc        D_map_akropolis_8017AA00;
static AreaObjectPlace D_map_akropolis_8017BE1C[6];
static AreaObjectPlace D_map_akropolis_8017BE7C[2];
static AreaObjectPlace D_map_akropolis_8017BE9C[5];
static AreaObjectPlace D_map_akropolis_8017BEEC[1];
static AreaObjectPlace D_map_akropolis_8017BEFC[2];
static AreaObjectPlace D_map_akropolis_8017BF1C[4];
static AreaObjectPlace D_map_akropolis_8017BF5C[2];
static AreaObjectPlace D_map_akropolis_8017BF7C[2];
static AreaObjectPlace D_map_akropolis_8017BF9C[3];
static AreaObjectPlace D_map_akropolis_8017BFCC[2];
static AreaObjectPlace D_map_akropolis_8017BFEC[2];
static AreaObjectPlace D_map_akropolis_8017C00C[2];
static AreaObjectPlace D_map_akropolis_8017C02C[1];
static AreaObjectPlace D_map_akropolis_8017C03C[10];

void func_map_akropolis_80179988(u8* arg0);
s32  func_map_akropolis_80179FC8(s32 arg0, s32 arg1);
s32  func_map_akropolis_8017A038(void);

/// MDEC buffer layout hook for the Akropolis map, reached from
/// `Mdec_SetupBuffers` (main) for the stream kinds this overlay plays. Every
/// kind parks the two VLC buffers (`D_8006AC50`) and the two decode buffers
/// (`D_8006AC48`) around the frame allocation; they differ in how far apart
/// the halves sit — one frame (kinds 6/9/10), one and a half (11/14) — and in
/// whether they also resize the display. `D_8006AC44` always ends up one full
/// frame past the second decode buffer.
void func_map_akropolis_80179988(u8* arg0)
{
    CdCmdQueue* q = &gCdCmdQueue;
    s16         one;
    s32         strideA;
    s32         strideB;
    s32         halfA;
    s32         halfB;

    switch (arg0[2]) {
        case 5:
            q->movieVramStaging = 1;
            q->movieStagingX    = 0x2C0;
            D_8006AC3C          = 0;
            q->movieStagingY    = 0;
            D_8006AC50[0]       = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1]       = (u_long*)D_8006AC40;
            D_8006AC48[1]       = (u_long*)((u8*)D_8006AC40 + D_8006AC5A * D_8006AC6C);
            D_8006AC48[0]       = D_8006AC48[1];
            break;
        case 8:
            /* The loop note pins `li 1` at the top of the block; without it the
               scheduler sinks it past the two display-size stores. */
            do {
                one = 1;
            } while (0);
            q->movieStagingX       = 0x180;
            q->movieStagingY       = 0x100;
            D_8006AC5C             = one;
            q->movieVramStaging    = one;
            strideA                = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0]          = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC48[0]          = (u_long*)D_8006AC40;
            D_8006AC50[1]          = (u_long*)((u8*)D_8006AC50[0] + strideA);
            D_8006AC48[1]          = (u_long*)((u8*)D_8006AC48[0] + strideA);
            gGameSession->field_80 = 0;
            q->field_24A           = one;
            break;
        case 6:
        case 9:
        case 10:
            strideB       = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + strideB);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + strideB);
            break;
        case 11:
            halfA         = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + halfA * 3 / 2);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + halfA * 2);
            break;
        case 14:
            halfB         = D_8006AC5A * D_8006AC6C;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC48[0] = (u_long*)D_8006AC40;
            D_8006AC50[1] = (u_long*)((u8*)D_8006AC50[0] + halfB * 3 / 2);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + halfB * 2);
            break;
    }
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    gGameSession->field_7C = 0;
    gGameSession->field_7E = 0;
}

/// Draws one row of the Akropolis map's key-item list: the item's name at the
/// row's position, previewed while the row is highlighted. Confirming on the
/// selected row opens the item-detail panel `D_8010EAB4[45]`; picking the row whose
/// item is 0x10C also records that choice in `D_map_akropolis_8017A9A8`, which
/// `func_map_akropolis_8017A038` reports back to the caller.
static void func_map_akropolis_80179C50(UiList* arg0, UiObject* arg1)
{
    s32 item;
    s32 sel;

    item = D_map_akropolis_8017A9AC[arg0->currentItemIndex];
    textDrawUiLine(arg1, arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, itemGetText(item, ITEM_TEXT_NAME, 0), arg0->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (((arg1->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (arg1->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
        }
    }
    sel = arg0->rowInputEnabled;
    if (sel == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[45], item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            if (item == 0x10C) {
                D_map_akropolis_8017A9A8 = sel;
            }
        }
    }
}

/// Per-frame handler for the Akropolis key-item map panel: draws the "Key Item"
/// heading, lays the key-item list out over the panel on the first frame, then
/// updates it. Cancel/menu asks the parent to close (`result` cancel); once the
/// spawned item-detail child reports cancel or confirm the child tree is torn down and the
/// panel goes back to its active state.
static void func_map_akropolis_80179D78(Task* task)
{
    UiObject* obj;
    UiList*   list;
    UiObject* child;
    s32       result;

    list        = &D_map_akropolis_8017A9C0;
    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(obj)->panel, D_map_akropolis_8017997C);
    if (task->state == 0) {
        uiFitPanelToList(list, &(obj)->panel);
        list->flags           = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        task->spawnArg1.value = -1;
        task->state          += 1;
    }
    uiUpdateList(list, &obj->panel);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->result = USER_INTERFACE_RESULT_CANCEL;
    }
    if (task->firstChild != NULL) {
        child  = task->firstChild->spawnArg2.pointer;
        result = child->result;
        switch (result) {
            case USER_INTERFACE_RESULT_CONFIRM:
                uiStartTreeClosing(child, child->owner);
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case USER_INTERFACE_RESULT_CANCEL:
                obj->result = result;
                break;
        }
    }
}

/// Task driving the Akropolis key-item map screen: spawns the UI tree
/// `D_map_akropolis_8017A9E4`, freezes the game's frame timing while it is up,
/// then tears it down and releases the screen once the tree reports cancel or confirm.
static void func_map_akropolis_80179E8C(Task* task)
{
    UiObject* obj;
    s16       result;

    if (task->state == 0) {
        stageEnsureHeapTaskPrimitiveBuffer();
        itemMenuClearPreviewItems();
        obj = uiSpawnObject(&D_map_akropolis_8017A9E4, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = obj;
        task->state            += 1;
    }

    if (task->state == 1) {
        obj    = task->spawnArg2.pointer;
        result = obj->result;
        if ((result == USER_INTERFACE_RESULT_CANCEL) || (result == USER_INTERFACE_RESULT_CONFIRM)) {
            uiStartTreeClosing(obj, obj->owner);
            task->killCountdown = 0xA;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown -= 1;
        if (task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(task);
            stageReleaseTaskPrimitiveBuffer();
            stageRequestModeTaskExit();
        }
    }
}

s32 func_map_akropolis_80179FC8(s32 arg0, s32 arg1)
{
    s32* p;
    s32  i;

    if (arg1 == 0) {
        i = 0;
        p = D_map_akropolis_8017A9AC;
        do {
            inventorySetCollectedBit(*p++);
            i++;
        } while (i < 4);
        D_map_akropolis_8017A9A8 = 0;
    }
    displayQueueModeTask(&D_map_akropolis_8017AA00, 0, 0, STAGE_ENTRY_RELOAD);
    return 1;
}

s32 func_map_akropolis_8017A038(void)
{
    return D_map_akropolis_8017A9A8;
}

GfxImageSlot D_map_akropolis_8017A048[22] = {
    { NULL, 0 },
    GFX_IMAGE_SLOT(0x4AF80),
    GFX_IMAGE_SLOT(0x4DB90),
    GFX_IMAGE_SLOT(0x4D340),
    GFX_IMAGE_SLOT(0x461C0),
    GFX_IMAGE_SLOT(0x3C030),
    GFX_IMAGE_SLOT(0x4E910),
    GFX_IMAGE_SLOT(0x56FA0),
    GFX_IMAGE_SLOT(0x50630),
    GFX_IMAGE_SLOT(0x50660),
    GFX_IMAGE_SLOT(0x51060),
    GFX_IMAGE_SLOT(0x4EB10),
    GFX_IMAGE_SLOT(0x4C9F0),
    GFX_IMAGE_SLOT(0x4ED80),
    GFX_IMAGE_SLOT(0x435E0),
    GFX_IMAGE_SLOT(0x51250),
    GFX_IMAGE_SLOT(0x4BC60),
    GFX_IMAGE_SLOT(0x4F450),
    GFX_IMAGE_SLOT(0x4F110),
    GFX_IMAGE_SLOT(0x3A8E0),
    GFX_IMAGE_SLOT(0x3FAD0),
    GFX_IMAGE_SLOT(0x582D0),
};

s32 D_map_akropolis_8017A0F8[21] = {
    0,
    0,
    0,
    0,
    0,
    0x2D000,
    0x25200,
    0,
    0x18C00,
    0x25200,
    0x1B000,
    0x28380,
    0,
    0,
    0x21000,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 D_map_akropolis_8017A14C[8] = { 0, 0x15, 0x15, 0xFF };

MenuMapArea D_map_akropolis_8017A154[22] = {
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0x196E, 0x1FF, 0xFFE2, 0xFFBA, 0xE6, 0xC8, 1 },
    { -0x1480, 0x42C, 0x28, 0xFFBD, 0xE6, 0xC8, 1 },
    { 0xF42, 0x300, 0xFFE9, 0xFFDD, 0xC8, 0xC8, 1 },
    { 0x731, -0xCB8, 0xFFCE, 0x19, 0xC8, 0xC8, 1 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { -0x2E, -0xB3D, 0xFFDD, 0x19, 0xC8, 0xC8, 1 },
    { 0xBC4, -0x2BA, 0xFFE0, 0x23, 0xC8, 0x12C, 1 },
    { -0xF38, 0xD8A, 0xFFEC, 0xFFFB, 0xC8, 0xE6, 1 },
    { -0xFA4, 0x355, 0x19, 0xFFD8, 0xC8, 0x96, 1 },
    { -0x8F8, -0x2BF2, 0x4E, 7, 0xC8, 0xBE, 1 },
    { 0x282, 0x23A0, 0x28, 6, 0xDC, 0xCD, 1 },
    { -0x261A, -0x1F86, 0x2B, 0x4A, 0x96, 0xC8, 1 },
    { -0x1D22, -0x25F0, 0x3C, 0x2F, 0xE6, 0xC8, 1 },
    { 0x282, 0x23A0, 0x28, 0xF, 0xD2, 0xE1, 1 },
    { 0xA44, -0xB7, 0xFFD3, 0x40, 0xA5, 0xCD, 1 },
    { -0x18F3, 0x18A2, 0xFFDE, 0xFFE2, 0xC8, 0xD2, 2 },
    { 0x1508, 0x40F, 0xFFDC, 0xFFC0, 0xC8, 0xC8, 1 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_NONE },
    { 0x2100, -0xEF4, 0xFFE4, 0xFFFC, 0xC8, 0xC8, 3 },
    { 0x2B5C, 0x1450, 0x6E, 1, 0xC8, 0xC8, 3 },
    { 0, 0, 0, 0, 0, 0, MENU_MAP_AREA_PAGE_END },
};

MenuMapAreaShape D_map_akropolis_8017A288[21] = {
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012EFD8, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F128, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F224, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F2D0, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F360, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F3F0, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F4F4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F5D4, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F684, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F76C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F7FC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F88C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F91C, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012F9AC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_02_8012EFA0, 2, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { &D_mappic_s1_00_8012FAFC, 1, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, MENU_MAP_AREA_SHAPE_PAGE_NONE, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
    { NULL, 3, MENU_MAP_AREA_SHAPE_PAIRED_AREA_NONE },
};

MenuMapMarker D_map_akropolis_8017A330[15] = {
    { 1, MENU_MAP_MARKER_AREA_ANY, -33, -69 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 33, -69 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -20, -7 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 20, -7 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 43, -42 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -80, -7 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -62, 34 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -52, 29 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -35, 29 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -28, 34 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 41, 76 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 59, 54 },
    { 1, MENU_MAP_MARKER_AREA_ANY, 26, 65 },
    { 1, MENU_MAP_MARKER_AREA_ANY, -44, 65 },
    { 0, 0, 0, 0 },
};

MenuMapIcon D_map_akropolis_8017A38C[6] = {
    { 1, 1, MENU_MAP_ICON_KIND_TELEPHONE, 0, 31, -77 },
    { 1, 0xF, MENU_MAP_ICON_KIND_TELEPHONE, 0, -76, 53 },
    { 3, 0x13, MENU_MAP_ICON_KIND_TELEPHONE, 0, -110, -26 },
    { 3, 0x13, 0, GAME_FLAG_STORY_CHAPTER, -57, -7 },
    { 1, 4, MENU_MAP_ICON_KIND_OBJECTIVE, 2, -77, 17 },
    { 0, 0, 0, 0, 0, 0 },
};

MenuMapAreaName D_map_akropolis_8017A3BC[21] = {
    { "Square" },
    { "East elevator hall" },
    { "Patio" },
    { "Cafeteria" },
    { "Plaza" },
    { "Security room" },
    { "Hallway" },
    { "Fountain" },
    { "Forked road" },
    { "Observatory" },
    { "Promenade" },
    { "Sanctuary" },
    { "Roof garden" },
    { "Bridge" },
    { "Fire escape" },
    { "Helicopter landing pad" },
    { "West elevator hall" },
    { "" },
    { "MIST parking" },
    { "Shooting gallery" },
    { "" },
};

static AreaObjectSpawn D_map_akropolis_8017A65C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A66C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A67C[3] = {
    { 4, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 0x62 } }, func_acropolis_cafeteria_8018286C, { &gAcropolisCafeteriaModel0F7A4 } } },
    { 0x107, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 0x62 } }, func_acropolis_cafeteria_801827C4, { &gAcropolisCafeteriaModel0FDFC } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A6AC[2] = {
    { 0x701, { { { TASK_BODY_TMD, 0x62 } }, Gp_ItemPickupTilt, { &gAcropolisSecurityRoomAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A6CC[2] = {
    { 0x104, { { { TASK_BODY_TMD, 0x62 } }, func_acropolis_hallway_8017E120, { &gAcropolisHallwayModel01AE0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A6EC[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A6FC[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A70C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A71C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A72C[3] = {
    { 0x103, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 0x62 } }, func_acropolis_sanctuary_80180264, { &gAcropolisSanctuaryModel09584 } } },
    { 0x702, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 0x62 } }, Gp_ItemPickupTilt, { &gAcropolisSanctuaryModel090F0 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A75C[2] = {
    { 0x105, { { { TASK_BODY_TMD, 0x62 } }, func_acropolis_roof_garden_80180160, { &gAcropolisRoofGardenModel09868 } } },
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A77C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A78C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectSpawn D_map_akropolis_8017A79C[1] = {
    { AREA_OBJECT_SPAWN_END },
};

static AreaObjectPlace D_map_akropolis_8017A7AC[2] = {
    { 0x28, 0x204, 0, 1, -0x14DC, -0xB40, -0x76C },
    { 0xFFFF },
};

static AreaObjectSpawn D_map_akropolis_8017A7CC[3] = {
    { 0x204, { { { TASK_BODY_TMD, 0x62 } }, acropolisHelicopterLandingPadLiftTask, { &gAcropolisHelicopterLandingPadModel0547C } } },
    { 0xA4, { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 0x62 } }, func_acropolis_helicopter_landing_pad_801822B0, { &gAcropolisHelicopterLandingPadModel0A8E8 } } },
    { AREA_OBJECT_SPAWN_END },
};

AreaObjectRoom D_map_akropolis_8017A7FC[22] = {
    { { NULL }, NULL },
    { { D_map_akropolis_8017BE1C }, D_map_akropolis_8017A65C },
    { { D_map_akropolis_8017BE7C }, D_map_akropolis_8017A66C },
    { { NULL }, NULL },
    { { D_map_akropolis_8017BE9C }, D_map_akropolis_8017A67C },
    { { NULL }, NULL },
    { { D_map_akropolis_8017BEEC }, D_map_akropolis_8017A6AC },
    { { D_map_akropolis_8017BEFC }, D_map_akropolis_8017A6CC },
    { { D_map_akropolis_8017BF1C }, D_map_akropolis_8017A6EC },
    { { D_map_akropolis_8017BF5C }, D_map_akropolis_8017A6FC },
    { { D_map_akropolis_8017BF7C }, D_map_akropolis_8017A70C },
    { { D_map_akropolis_8017BF9C }, D_map_akropolis_8017A71C },
    { { D_map_akropolis_8017BFCC }, D_map_akropolis_8017A72C },
    { { D_map_akropolis_8017BFEC }, D_map_akropolis_8017A75C },
    { { NULL }, NULL },
    { { D_map_akropolis_8017C00C }, D_map_akropolis_8017A77C },
    { { D_map_akropolis_8017A7AC }, D_map_akropolis_8017A7CC },
    { { D_map_akropolis_8017C02C }, D_map_akropolis_8017A78C },
    { { NULL }, NULL },
    { { D_map_akropolis_8017C03C }, D_map_akropolis_8017A79C },
    { { NULL }, NULL },
    { { .sentinel = AREA_OBJECT_ROOM_END }, NULL },
};

TaskDesc D_map_akropolis_8017A8AC[] = {
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_square_80182308, { .value = GP_TASK_LOC_KEY(1, 1, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_east_elevator_hall_8017F55C, { .value = GP_TASK_LOC_KEY(1, 2, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_patio_8017DF8C, { .value = GP_TASK_LOC_KEY(1, 3, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_cafeteria_8017E424, { .value = GP_TASK_LOC_KEY(1, 4, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_security_room_8017D984, { .value = GP_TASK_LOC_KEY(1, 6, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_hallway_8017D7D0, { .value = GP_TASK_LOC_KEY(1, 7, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_fountain_8017D9C4, { .value = GP_TASK_LOC_KEY(1, 8, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_forked_road_8017D9CC, { .value = GP_TASK_LOC_KEY(1, 9, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_observatory_8017D950, { .value = GP_TASK_LOC_KEY(1, 10, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_promenade_8017DA4C, { .value = GP_TASK_LOC_KEY(1, 11, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_sanctuary_8017D9E8, { .value = GP_TASK_LOC_KEY(1, 12, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_roof_garden_8017DC74, { .value = GP_TASK_LOC_KEY(1, 13, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_bridge_8017DA0C, { .value = GP_TASK_LOC_KEY(1, 14, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_fire_escape_8017FF24, { .value = GP_TASK_LOC_KEY(1, 15, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_helicopter_landing_pad_8017EB00, { .value = GP_TASK_LOC_KEY(1, 16, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_acropolis_west_elevator_hall_8017F5F4, { .value = GP_TASK_LOC_KEY(1, 17, 1) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_mist_r18_8017ED64, { .value = GP_TASK_LOC_KEY(1, 18, 1) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_mist_parking_80182898, { .value = GP_TASK_LOC_KEY(1, 19, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_mist_shooting_gallery_8018018C, { .value = GP_TASK_LOC_KEY(1, 20, 0) } },
    { { { TASK_BODY_NONE, 0x20 } }, func_mist_r21_8017D708, { .value = GP_TASK_LOC_KEY(1, 21, 0) } },
    { { { TASK_DESC_END, 0x20 } }, NULL, { 0 } },
};

/// Which key-item row the panel last had selected.
static s32 D_map_akropolis_8017A9A8 = 0;

/// The four text ids the key-item rows draw, in row order.
static s32 D_map_akropolis_8017A9AC[4] = { 0x109, 0x10A, 0x10B, 0x10C };

/// One-entry row-draw table for the list below; `UiList.rowCallbacks` points here.
static UiListRowCallback D_map_akropolis_8017A9BC[1] = { func_map_akropolis_80179C50 };

/// The key-item list: four rows of one line each, 0x0F tall, everything else
/// filled in at runtime by the list reset.
static UiList D_map_akropolis_8017A9C0 = {
    D_map_akropolis_8017A9BC,
    4,
    4,
    1,
    0x0F,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    { 0 },
    0,
    0,
};

/// The panel `uiSpawnObject` builds for the key-item view: 0xFF9C x 0xFFD8,
/// 0xC8 x 0x3C, drawn by func_map_akropolis_80179D78.
static UiObjectDesc D_map_akropolis_8017A9E4 = {
    USER_INTERFACE_PANEL_TITLE_STYLE,
    { -100,
      -40,
      0xC8,
      0x3C },
    0x3C,
    0,
    TASK_BODY_NONE,
    0xC0,
    func_map_akropolis_80179D78,
    0,
};

/// The display-mode task `displayQueueModeTask` seeds for this map.
static TaskDesc D_map_akropolis_8017AA00 = { { { TASK_BODY_NONE, 0xC0 } }, func_map_akropolis_80179E8C, 0 };

u16 D_map_akropolis_8017AA0C[14] = {
    0x1F7,
    0x1F6,
    0x1F0,
    0x1EF,
    0x1EE,
    0x1F5,
    0x1F4,
    0x1F3,
    0x1F2,
    0x1F1,
    0x1ED,
    0x1EC,
    0x1EB,
    0x1EA,
};

WorldCoordRoomLighting* D_map_akropolis_8017AA28[20] = {
    D_acropolis_square_80183BB4,
    D_acropolis_east_elevator_hall_80186338,
    D_acropolis_patio_80182ED4,
    D_acropolis_cafeteria_801875C4,
    D_acropolis_plaza_801988D0,
    D_acropolis_security_room_801839E8,
    D_acropolis_hallway_8017E270,
    D_acropolis_fountain_8017E858,
    D_acropolis_forked_road_80182270,
    D_acropolis_observatory_8017FEFC,
    D_acropolis_promenade_80181BCC,
    D_acropolis_sanctuary_80182804,
    D_acropolis_roof_garden_80184CA4,
    D_acropolis_bridge_80189A8C,
    D_acropolis_fire_escape_80181DC4,
    D_acropolis_helicopter_landing_pad_80184F44,
    D_acropolis_west_elevator_hall_8018503C,
    D_mist_r18_80186624,
    D_mist_parking_801915C8,
    gMistShootingGalleryRoomLightingTable,
};

static WorldCollisionRoomResources* D_map_akropolis_8017AA78[20] = {
    D_acropolis_square_80183B9C,
    D_acropolis_east_elevator_hall_80186320,
    D_acropolis_patio_80182E68,
    D_acropolis_cafeteria_8018753C,
    D_acropolis_plaza_801988B8,
    D_acropolis_security_room_801839D0,
    D_acropolis_hallway_8017E258,
    D_acropolis_fountain_8017E814,
    D_acropolis_forked_road_80182214,
    D_acropolis_observatory_8017FEC8,
    D_acropolis_promenade_80181B90,
    D_acropolis_sanctuary_801827EC,
    D_acropolis_roof_garden_80184C8C,
    D_acropolis_bridge_80189A54,
    D_acropolis_fire_escape_80181DAC,
    D_acropolis_helicopter_landing_pad_80184F10,
    D_acropolis_west_elevator_hall_80185024,
    D_mist_r18_8018660C,
    D_mist_parking_8019155C,
    D_mist_shooting_gallery_801853A8,
};

WorldCollisionStageResources D_map_akropolis_8017AAC8 = { D_map_akropolis_8017AA78 };

static SpriteView* D_map_akropolis_8017AACC[20] = {
    D_acropolis_square_8018857C,
    D_acropolis_east_elevator_hall_80187870,
    D_acropolis_patio_80186360,
    D_acropolis_cafeteria_8018C48C,
    D_acropolis_plaza_80198A08,
    D_acropolis_security_room_80184C50,
    D_acropolis_hallway_8017EC2C,
    D_acropolis_fountain_8018375C,
    D_acropolis_forked_road_801844E0,
    D_acropolis_observatory_80183300,
    D_acropolis_promenade_80185FB4,
    D_acropolis_sanctuary_801860C8,
    D_acropolis_roof_garden_80186648,
    D_acropolis_bridge_8018FFA4,
    D_acropolis_fire_escape_80182E18,
    D_acropolis_helicopter_landing_pad_80187824,
    D_acropolis_west_elevator_hall_80186408,
    D_mist_r18_80186B60,
    D_mist_parking_8019399C,
    D_mist_shooting_gallery_8018BD10,
};

SpriteAreaTable D_map_akropolis_8017AB1C = { D_map_akropolis_8017AACC };

DirectionWarpEntry* D_map_akropolis_8017AB20[20] = {
    D_acropolis_square_80183BBC,
    D_acropolis_east_elevator_hall_80186340,
    D_acropolis_patio_80182EEC,
    D_acropolis_cafeteria_801875E4,
    D_acropolis_plaza_80198A68,
    D_acropolis_security_room_801839F0,
    D_acropolis_hallway_8017E278,
    D_acropolis_fountain_8017E868,
    D_acropolis_forked_road_80182288,
    D_acropolis_observatory_8017FF0C,
    D_acropolis_promenade_80181BDC,
    D_acropolis_sanctuary_8018280C,
    D_acropolis_roof_garden_80184CAC,
    D_acropolis_bridge_80189AB4,
    D_acropolis_fire_escape_80181DCC,
    D_acropolis_helicopter_landing_pad_80184F4C,
    D_acropolis_west_elevator_hall_80185044,
    D_mist_r18_8018662C,
    D_mist_parking_801915E8,
    D_mist_shooting_gallery_801853C8,
};

static ViewCount* D_map_akropolis_8017AB70[20] = {
    D_acropolis_square_80183BB0,
    D_acropolis_east_elevator_hall_80186334,
    D_acropolis_patio_80182ECC,
    D_acropolis_cafeteria_801875BC,
    D_acropolis_plaza_801988CC,
    D_acropolis_security_room_801839E4,
    D_acropolis_hallway_8017E26C,
    D_acropolis_fountain_8017E854,
    D_acropolis_forked_road_80182268,
    D_acropolis_observatory_8017FEF8,
    D_acropolis_promenade_80181BC8,
    D_acropolis_sanctuary_80182800,
    D_acropolis_roof_garden_80184CA0,
    D_acropolis_bridge_80189A88,
    D_acropolis_fire_escape_80181DC0,
    D_acropolis_helicopter_landing_pad_80184F40,
    D_acropolis_west_elevator_hall_80185038,
    D_mist_r18_80186620,
    D_mist_parking_801915C0,
    D_mist_shooting_gallery_801853BC,
};

ViewCountTable D_map_akropolis_8017ABC0 = { D_map_akropolis_8017AB70 };

static ViewCamera* D_map_akropolis_8017ABC4[20] = {
    D_acropolis_square_80188630,
    D_acropolis_east_elevator_hall_80187A5C,
    D_acropolis_patio_80186D5C,
    D_acropolis_cafeteria_8018C5AC,
    D_acropolis_plaza_801988D8,
    D_acropolis_security_room_80184D10,
    D_acropolis_hallway_8017EC68,
    D_acropolis_fountain_80183864,
    D_acropolis_forked_road_80184E88,
    D_acropolis_observatory_80183360,
    D_acropolis_promenade_80186050,
    D_acropolis_sanctuary_80186188,
    D_acropolis_roof_garden_80186BF4,
    D_acropolis_bridge_80190A24,
    D_acropolis_fire_escape_80182E90,
    D_acropolis_helicopter_landing_pad_80187968,
    D_acropolis_west_elevator_hall_801869FC,
    D_mist_r18_8018671C,
    D_mist_parking_80192228,
    D_mist_shooting_gallery_8018998C,
};

ViewCameraTable D_map_akropolis_8017AC14 = { D_map_akropolis_8017ABC4 };

static u8** D_map_akropolis_8017AC18[20] = {
    D_acropolis_square_80183BAC,
    D_acropolis_east_elevator_hall_80186330,
    D_acropolis_patio_80182EC0,
    D_acropolis_cafeteria_801875AC,
    D_acropolis_plaza_801988C8,
    D_acropolis_security_room_801839E0,
    D_acropolis_hallway_8017E268,
    D_acropolis_fountain_8017E84C,
    D_acropolis_forked_road_8018225C,
    D_acropolis_observatory_8017FEF0,
    D_acropolis_promenade_80181BC0,
    D_acropolis_sanctuary_801827FC,
    D_acropolis_roof_garden_80184C9C,
    D_acropolis_bridge_80189A80,
    D_acropolis_fire_escape_80181DBC,
    D_acropolis_helicopter_landing_pad_80184F3C,
    D_acropolis_west_elevator_hall_80185034,
    D_mist_r18_8018661C,
    D_mist_parking_801915B0,
    D_mist_shooting_gallery_801853B8,
};

ViewIndexTable D_map_akropolis_8017AC68 = { D_map_akropolis_8017AC18 };

WorldCollisionSurfaceProperties** D_map_akropolis_8017AC6C[20] = {
    D_acropolis_square_80188868,
    D_acropolis_east_elevator_hall_80187B74,
    D_acropolis_patio_8018703C,
    D_acropolis_cafeteria_8018CA2C,
    D_acropolis_plaza_80199F28,
    D_acropolis_security_room_80184FA0,
    D_acropolis_hallway_8017ED40,
    D_acropolis_fountain_80183B90,
    D_acropolis_forked_road_801850A4,
    D_acropolis_observatory_801834DC,
    D_acropolis_promenade_801862B0,
    D_acropolis_sanctuary_801863F8,
    D_acropolis_roof_garden_80186DB0,
    D_acropolis_bridge_80190C34,
    D_acropolis_fire_escape_80183020,
    D_acropolis_helicopter_landing_pad_80187DC8,
    D_acropolis_west_elevator_hall_80186AC4,
    D_mist_r18_80186E70,
    D_mist_parking_801952F0,
    D_mist_shooting_gallery_8018E09C,
};

AreaPlacement D_map_akropolis_8017ACBC[2] = {
    { 0x13, 0, 0, 0x1770, -0x898, 0xC8F, 0x800, 0, -1, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017ACDC[2] = {
    { 0x6E, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017ACFC[3] = {
    { 0x6B, 0, 0, -0xB54, 0, 0x640, 0x600, 0, 4, 6, 0 },
    { 0x13, 4, 0, -0xAF0, 0, 0x5DC, -0x200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AD2C[5] = {
    { 0xA, 0, 2, -0x1770, -0xEC4, -0x76C, 0, 0, 0, 2, 0 },
    { 0xA, 0, 2, -0x13EC, -0xEC4, -0x76C, 0, 0, 0, 2, 0 },
    { 0x13, 4, 2, -0x1B58, 0, 0x2BC, 0xAF0, 0, 0, 2, 0 },
    { 0x13, 4, 2, -0x1C84, 0, -0x320, -0x258, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AD7C[7] = {
    { 8, 0, 0, -0x170C, -0x3E8, 0x5DC, 0, 0, 0, 2, 0 },
    { 8, 0, 0, -0x1644, -0x3E8, 0x5DC, -0xC8, 0, 0, 2, 0 },
    { 8, 0, 0, -0x17D4, -0x3E8, 0x5DC, 0xC8, 0, 0, 2, 0 },
    { 7, 0, 0, -0x1E14, 0, 0x12C, 0xC00, 0, 2, 4, 1 },
    { 7, 0, 0, -0x1E14, 0, 0x320, 0x9C4, 0, 2, 4, 1 },
    { 7, 0, 0, -0x1D4C, 0, 0x1F4, 0xAF0, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017ADEC[5] = {
    { 0xA, 4, 0, -0x1B58, 0, 0x2BC, 0xAF0, 0, 0, 2, 0 },
    { 0xA, 4, 0, -0x1C84, 0, -0x320, -0x258, 0, 0, 2, 0 },
    { 8, 0, 0, -0x1838, -0x320, 0x492, 0x10E, 0, 2, 4, 1 },
    { 8, 0, 0, -0x1518, -0x3E8, 0x44C, 0xED8, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AE3C[3] = {
    { 0xA, 0, 0, 0x64, 1, -0x3E8, 0x1CC, 0, 0, 2, 0 },
    { 0xA, 0, 0, -0x9C4, 0, 0x1F4, -0x200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AE6C[3] = {
    { 9, 4, 0x212, 0x14AA, 1, -0x10E, 0x456, 0, 0, 2, 0 },
    { 9, 0, 0x1212, 0x14DC, 1, 0x33E, 0x456, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AE9C[4] = {
    { 0xA, 1, 0x20, -0xD48, -0x12C, -0x708, 0, 0, 0, 2, 0 },
    { 0x1D, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { 0x66, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AEDC[4] = {
    { 0xA, 1, 0x20, -0xD48, -0x12C, -0x708, 0, 0, 0, 2, 0 },
    { 0x13, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { 0x66, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AF1C[2] = {
    { 0x12, 0x14, 2, 0x64, -0x12C, 0, 0x7D0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AF3C[3] = {
    { 0x12, 0x14, 2, 0x64, -0x12C, 0, 0x7D0, 0, 0, 2, 0 },
    { 0x12, 0x10, 2, 0x64, -0x12C, 0x5DC, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AF6C[6] = {
    { 0xC, 0, 0, 0x9C4, -0x12C, 0x226, 0x898, 0, 0, 2, 0 },
    { 0xC, 0, 0, 0x104, -0x12C, 0x640, 0x71E, 0, 0, 2, 0 },
    { 0xC, 0, 0, 0x47E, -0x12C, -0xA0, 0x762, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x302, -0x12C, -0x816, 0xE9C, 0, 0, 2, 0 },
    { 0xC, 1, 0, 0x4B0, -0x12C, 0x6C2, 0x15E, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AFCC[2] = {
    { 0x39, 3, 1, -0xED8, -0x12C, 0x3E8, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017AFEC[3] = {
    { 0x6D, 0, 0, 0x3520, 0, 0x11F8, 0x6AA, 2, 2, 4, 0 },
    { 0x6C, 0, 0, 0x3CA, 0, 0x393A, 0x800, 1, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B01C[3] = {
    { 0x66, 0, 0, -0x1444, 0, -0xAF0, 0x100, 0, 0, 2, 0 },
    { 0x6E, 0, 0, -0x127A, 0, -0xD72, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B04C[9] = {
    { 7, 0, 0, 0x3E8, -0x3E8, -0x3E8, 0xC00, 0, 0, 2, 0 },
    { 7, 0, 0, 0x3E8, -0x3E8, -0x1F4, 0x800, 0, 0, 2, 0 },
    { 7, 0, 0, 0x320, -0x3E8, -0x384, 0x400, 0, 0, 2, 0 },
    { 7, 0, 0, 0x3E8, -0x3E8, 0x3E8, 0, 0, 0, 2, 0 },
    { 7, 0, 0, 0x1F4, -0x3E8, 0x384, 0xED8, 0, 0, 2, 0 },
    { 7, 0, 0, -0x44C, -0x3E8, -0x3E8, 0x5DC, 0, 0, 2, 0 },
    { 7, 0, 0, -0x320, -0x3E8, -0x320, 0x898, 0, 0, 2, 0 },
    { 7, 0, 0, -0x258, -0x3E8, -0x384, 0xA8C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B0DC[2] = {
    { 0x12, 0x10, 0, 0, -0x3E8, -0x1F4, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B0FC[2] = {
    { 0x12, 0x10, 0, 0, -0x3E8, -0x1F4, 0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B11C[6] = {
    { 0x12, 0x10, 2, 0, -0x3E8, -0x1F4, 0x400, 0, 0, 2, 0 },
    { 7, 0, 0, 0x3E8, -0x3E8, -0x3E8, 0, 0, 2, 4, 1 },
    { 7, 0, 0, 0x3E8, -0x3E8, -0x1F4, 0x800, 0, 2, 4, 1 },
    { 7, 0, 0, 0x3E8, -0x3E8, 0x3E8, 0x800, 0, 2, 4, 1 },
    { 7, 0, 0, 0x1F4, -0x3E8, 0x3E8, 0x400, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B17C[3] = {
    { 0xB, 0, 0, -0xEB0, 0, -0x6A4, -0x200, 0, 0, 2, 0 },
    { 0xB, 0, 0, 0x208, 2, -0x1130, 0xE10, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B1AC[3] = {
    { 0x31, 0, 0, -0xEB0, 0, -0x6A4, -0x200, 0, 0, 2, 0 },
    { 0x31, 0, 0, 0x208, 2, -0x1130, 0xE10, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B1DC[2] = {
    { 0x13, 4, 0, -0x1F4, 1, -0x2BC, 0x44C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B1FC[5] = {
    { 0xA, 0, 0, -0xFA0, 0, -0x3E8, 0x320, 0, 0, 2, 0 },
    { 7, 0, 0, -0x1F4, 0, -0x4B0, 0x2BC, 0, 2, 4, 0 },
    { 7, 0, 0, 0, 0, -0x320, 0x400, 0, 2, 4, 0 },
    { 7, 0, 0, 0, 0, -0x12C, 0x514, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B24C[5] = {
    { 0x37, 3, 0xA, 0x2BC, 0, -0x1194, 0xE10, 0, 0, 2, 0 },
    { 0x37, 0, 0xA, 0x578, 0, -0x960, 0xD48, 0, 0, 2, 0 },
    { 0x37, 2, 0xA, 0x7D0, 0, 0, 0xC00, 0, 0, 2, 0 },
    { 0x37, 1, 0xA, 0x4B0, 0, 0x5DC, 0xB86, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B29C[5] = {
    { 0x14, 3, 1, -0x73A, 0, 0x320, 0x9F6, 0, 0, 2, 0 },
    { 7, 0, 0, -0x1F4, 0, -0x4B0, 0x2BC, 0, 3, 5, 7 },
    { 7, 0, 0, 0, 0, -0x320, 0x400, 0, 3, 5, 7 },
    { 7, 0, 0, 0, 0, -0x12C, 0x514, 0, 3, 5, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B2EC[5] = {
    { 0x1A, 3, 0xA, 0x2BC, 0, -0x1194, 0xE10, 0, 0, 2, 7 },
    { 0x1A, 0, 0xA, 0x578, 0, -0x960, 0xD48, 0, 0, 2, 7 },
    { 0x1A, 2, 0xA, 0x7D0, 0, 0, 0xC00, 0, 0, 2, 7 },
    { 0x1A, 1, 0xA, 0x4B0, 0, 0x5DC, 0xB86, 0, 0, 2, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B33C[2] = {
    { 0x12, 4, 0, -0x186, 1, -0x384, 0x44C, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B35C[12] = {
    { 7, 0, 0, -0x1820, 0, -0x1C0, 0, 0, 0, 2, 0 },
    { 7, 0, 0, -0x2E0, 0, -0x480, 0, 0, 0, 2, 2 },
    { 7, 0, 0, -0x1500, 0, -0xAA0, 0, 0, 0, 2, 4 },
    { 7, 0, 0, -0x4A0, 0, 0x520, 0, 0, 0, 2, 5 },
    { 7, 0, 0, -0x3C0, 0, 0x80, 0, 0, 0, 2, 6 },
    { 7, 0, 0, -0xA60, 0, -0x548, 0, 0, 0, 2, 7 },
    { 8, 0, 0, -0x1880, -0x320, -0x680, 0, 0, 0, 2, 0 },
    { 8, 0, 0, -0x1880, -0x320, -0x3E8, 0, 0, 0, 2, 2 },
    { 8, 0, 0, -0x1900, -0x320, -0xB20, 0, 0, 0, 2, 4 },
    { 8, 0, 0, -0x16BF, -0x320, -0x7A1, 0, 0, 0, 2, 6 },
    { 8, 0, 0, -0x14E0, -0x320, -0x6A0, 0, 0, 0, 2, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B41C[7] = {
    { 0x18, 0, 0, -0xA28, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x18, 0, 1, -0x1820, 0, -0x1C0, 0, 0, 0, 2, 1 },
    { 0x18, 0, 0, -0x2E0, 0, -0x480, 0, 0, 0, 2, 2 },
    { 0x18, 0, 1, -0x1500, 0, -0xAA0, 0, 0, 0, 2, 3 },
    { 0x18, 0, 0, -0x4A0, 0, 0x520, 0, 0, 0, 2, 4 },
    { 0x18, 0, 1, -0x3C0, 0, 0x80, 0, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B48C[7] = {
    { 0x19, 0, 0, -0xA28, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x19, 0, 1, -0x1820, 0, -0x1C0, 0, 0, 0, 2, 1 },
    { 0x19, 0, 1, -0x2E0, 0, -0x480, 0, 0, 0, 2, 2 },
    { 0x19, 0, 1, -0x1500, 0, -0xAA0, 0, 0, 0, 2, 3 },
    { 0x19, 0, 1, -0x4A0, 0, 0x520, 0, 0, 0, 2, 4 },
    { 0x19, 0, 1, -0x3C0, 0, 0x80, 0, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B4FC[5] = {
    { 0x1A, 0, 0, -0xA28, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x1A, 0, 1, -0x1518, 0, -0x7D0, 0x200, 0, 0, 2, 0 },
    { 0x1A, 0, 2, -0x1518, -0x1770, 0, 0, 0, 0, 2, 0 },
    { 0x1A, 0, 2, -0x1388, -0x1770, -0xA8C, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B54C[7] = {
    { 0x25, 0, 0, -0xA28, -0x3E8, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x25, 0, 1, -0x1500, -0x3E8, -0xAA0, 0, 0, 0, 2, 1 },
    { 0x25, 0, 2, -0x2E0, -0x3E8, -0x480, 0, 0, 0, 2, 2 },
    { 0x25, 0, 0, -0x3C0, -0x3E8, 0x80, 0, 0, 0, 2, 3 },
    { 0x25, 0, 0, -0x4A0, -0x3E8, 0x520, 0, 0, 0, 2, 4 },
    { 0x25, 0, 0, -0x1820, -0x3E8, -0x1C0, 0, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B5BC[3] = {
    { 0xF, 0, 0, -0xA28, -0x5DC, -0x3E8, 0, 0, 0, 2, 0 },
    { 0xF, 0, 1, -0x14B4, -0x5DC, -0x3E8, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B5EC[7] = {
    { 0x26, 0, 0, -0xA28, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x26, 0, 1, -0x1820, 0, -0x1C0, 0, 0, 0, 2, 1 },
    { 0x26, 0, 0, -0x2E0, 0, -0x480, 0, 0, 0, 2, 2 },
    { 0x26, 0, 1, -0x1500, 0, -0xAA0, 0, 0, 0, 2, 3 },
    { 0x26, 0, 0, -0x4A0, 0, 0x520, 0, 0, 0, 2, 4 },
    { 0x26, 0, 1, -0x3C0, 0, 0x80, 0, 0, 0, 2, 5 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B65C[2] = {
    { 0x10, 0, 0, -0xA60, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B67C[3] = {
    { 0x2E, 0, 0, -0x147D, 0, -0xCCF, 0, 0, 0, 2, 0 },
    { 0x2F, 0, 0, -0xA60, 0, -0x3E8, 0x3E8, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B6AC[3] = {
    { 0x46, 0, 0, -0xA60, 0, -0x3E8, 0, 0, 0, 2, 0 },
    { 0x48, 0, 0, 0x3C, 0, 0x380, 0x3E8, 0, 2, 3, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B6DC[3] = {
    { 0x13, 0, 0, -0x9C4, -0xBAE, -0x1450, 0x708, 0, 0, 2, 0 },
    { 0x13, 4, 0, -0x139E, -0xBAE, -0x1F97, -0x400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B70C[3] = {
    { 0xB, 1, 0, -0x640, -0xBAE, -0x1194, 0x800, 0, 0, 2, 0 },
    { 0xB, 0, 0, -0x1194, -0xBAE, -0x1F40, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B73C[10] = {
    { 0x25, 0, 0x1E, -0x76C, -0x2454, -0x1C84, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x8FC, -0x251C, -0x1B58, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0xA28, -0x24B8, -0x1CE8, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x640, -0x2580, -0x1C84, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x578, -0x251C, -0x1BBC, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x3E8, -0x24B8, -0x1A90, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x5DC, -0x2454, -0x1AF4, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x834, -0x251C, -0x1C20, 0, 0, 0, 2, 0 },
    { 0x25, 0, 0x1E, -0x6A4, -0x251C, -0x1B58, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B7DC[3] = {
    { 0x31, 1, 0, -0x640, -0xBAE, -0x1194, 0x800, 0, 0, 2, 0 },
    { 0x31, 0, 0, -0x1194, -0xBAE, -0x1F40, 0xC00, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B80C[2] = {
    { 0x12, 0, 0, -0x1194, -0xBAE, -0x1D4C, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B82C[3] = {
    { 0xB, 1, 0, -0x5DC, 0x28, -0x1770, 0x8FC, 0, 0, 2, 0 },
    { 0xB, 1, 0, -0x1F4, 0x28, -0x3E8, 0x800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B85C[8] = {
    { 7, 0, 0, -0x898, 0x28, -0x1A2C, 0x898, 0, 2, 4, 1 },
    { 7, 0, 0, -0x7D0, 0x28, -0x1CE8, 0x8FC, 0, 2, 4, 1 },
    { 7, 0, 0, -0x640, 0x28, -0x1B26, 0x9C4, 0, 2, 4, 1 },
    { 7, 0, 0, -0x708, 0x28, -0x1E14, 0xB86, 0, 2, 4, 1 },
    { 8, 0, 0, -0x898, -0x7D0, -0xAF0, 0xAF0, 0, 2, 4, 0 },
    { 8, 0, 0, -0x898, -0x708, -0xBB8, 0xC00, 0, 2, 4, 0 },
    { 8, 0, 0, -0x898, -0x898, -0xC80, 0xC80, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B8DC[11] = {
    { 0x37, 0, 0, -0x7D0, 0x28, 0x258, 0x258, 0, 0, 2, 0 },
    { 0x37, 0, 0, -0x640, 0x28, -0x384, 0x258, 0, 0, 2, 0 },
    { 0x37, 0, 0, -0x12C, 0x28, -0x258, -0xC8, 0, 0, 2, 0 },
    { 0x37, 0, 0, -0x672, 0x28, -0x1388, 0xC8, 0, 0, 2, 0 },
    { 8, 0, 0, -0x898, -0x7D0, -0xC8, 0xAF0, 0, 2, 4, 1 },
    { 8, 0, 0, -0x898, -0x708, -0x190, 0xC00, 0, 2, 4, 1 },
    { 8, 0, 0, -0x898, -0x898, -0x258, 0xC80, 0, 2, 4, 1 },
    { 8, 0, 0, 0x320, -0x9C4, -0x1068, 0x258, 0, 2, 4, 1 },
    { 8, 0, 0, 0x320, -0xAF0, -0xF3C, 0x400, 0, 2, 4, 1 },
    { 8, 0, 0, 0x320, -0x960, -0xE10, 0x640, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B98C[2] = {
    { 0x16, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017B9AC[11] = {
    { 0x1A, 0, 0, -0x7D0, 0x28, 0x258, 0x258, 0, 0, 2, 7 },
    { 0x1A, 0, 0, -0x640, 0x28, -0x384, 0x258, 0, 0, 2, 7 },
    { 0x1A, 0, 0, -0x12C, 0x28, -0x258, -0xC8, 0, 0, 2, 7 },
    { 0x1A, 0, 0, -0x672, 0x28, -0x1388, 0xC8, 0, 0, 2, 7 },
    { 8, 0, 0, -0x898, -0x7D0, -0xC8, 0xAF0, 0, 2, 4, 7 },
    { 8, 0, 0, -0x898, -0x708, -0x190, 0xC00, 0, 2, 4, 7 },
    { 8, 0, 0, -0x898, -0x898, -0x258, 0xC80, 0, 2, 4, 7 },
    { 8, 0, 0, 0x320, -0x9C4, -0x1068, 0x258, 0, 2, 4, 7 },
    { 8, 0, 0, 0x320, -0xAF0, -0xF3C, 0x400, 0, 2, 4, 7 },
    { 8, 0, 0, 0x320, -0x960, -0xE10, 0x640, 0, 2, 4, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BA5C[3] = {
    { 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x66, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BA8C[3] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x66, 0, 0, 0, 0, 0, 0, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BABC[6] = {
    { 0x6E, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x37, 0, 0xB, -0x1A2C, -0x1388, -0x1D4C, 0x320, 0, 2, 4, 0 },
    { 0x37, 1, 0xB, -0x1450, -0x1388, -0x1E14, 0xC8, 0, 2, 4, 0 },
    { 0x37, 2, 0xB, -0x1DB0, -0x1388, -0x2328, 0, 0, 2, 4, 0 },
    { 0x37, 3, 0xB, -0xF3C, -0x1388, -0x1644, 0xAF0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BB1C[8] = {
    { 0x37, 0, 3, -0x1A2C, -0x1388, -0x1D4C, 0x320, 0, 0, 2, 0 },
    { 0x37, 0, 3, -0x1450, -0x14B4, -0x1E14, 0xC8, 0, 0, 2, 0 },
    { 0x37, 0, 3, -0x1DB0, -0x15E0, -0x2328, 0, 0, 0, 2, 0 },
    { 0x37, 0, 2, -0xF3C, -0x170C, -0x1644, 0xAF0, 0, 0, 2, 0 },
    { 8, 0, 0, -0x157C, -0x7D0, -0x9C4, 0, 0, 2, 4, 1 },
    { 8, 0, 0, -0xFA0, -0x7D0, -0x9C4, 0, 0, 2, 4, 1 },
    { 8, 0, 0, -0x1388, -0x7D0, -0x9C4, 0, 0, 2, 4, 1 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BB9C[6] = {
    { 0x6E, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x1A, 0, 0xB, -0x1A2C, -0x1388, -0x1D4C, 0x320, 0, 2, 4, 7 },
    { 0x1A, 1, 0xB, -0x1450, -0x1388, -0x1E14, 0xC8, 0, 2, 4, 7 },
    { 0x1A, 2, 0xB, -0x1DB0, -0x1388, -0x2328, 0, 0, 2, 4, 7 },
    { 0x1A, 3, 0xB, -0xF3C, -0x1388, -0x1644, 0xAF0, 0, 2, 4, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BBFC[8] = {
    { 0x1A, 0, 3, -0x1A2C, -0x1388, -0x1D4C, 0x320, 0, 0, 2, 7 },
    { 0x1A, 0, 3, -0x1450, -0x14B4, -0x1E14, 0xC8, 0, 0, 2, 7 },
    { 0x1A, 0, 3, -0x1DB0, -0x15E0, -0x2328, 0, 0, 0, 2, 7 },
    { 0x1A, 0, 2, -0xF3C, -0x170C, -0x1644, 0xAF0, 0, 0, 2, 7 },
    { 8, 0, 0, -0x157C, -0x7D0, -0x9C4, 0xC00, 0, 2, 4, 7 },
    { 8, 0, 0, -0xFA0, -0x7D0, -0x9C4, 0xC00, 0, 2, 4, 7 },
    { 8, 0, 0, -0x1388, -0x7D0, -0x9C4, 0xC00, 0, 2, 4, 7 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BC7C[7] = {
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x7A0, 0, 0, 0, 2, 0 },
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x548, 0, 0, 0, 2, 0 },
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x610, 0, 0, 0, 2, 0 },
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x7A0, 0, 0, 0, 2, 0 },
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x548, 0, 0, 0, 2, 0 },
    { 0x29, 0, 0, -0x2EE0, 0x1F4, -0x76C, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BCEC[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BCFC[1] = {
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BD0C[2] = {
    { 0xA, 0, 0, 0x4B0, 0, -0x5DC, -0x200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BD2C[2] = {
    { 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BD4C[4] = {
    { 0x1B, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x90, 0, 0, 0, 0, 0, 0, 0, 3, 5, 0 },
    { 0xFE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BD8C[3] = {
    { 0x90, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 0x69, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BDBC[3] = {
    { 0x8F, 0, 0, 0x13A6, 0, -0x1630, -0x128, 0, 0, 2, 0 },
    { 0x73, 0, 0, 0x210C, 0, 0x5C, 0x7FF, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END },
};

AreaPlacement D_map_akropolis_8017BDEC[3] = {
    { 0x4C, 0, 0xFF, -0x1DB0, 0, 0x4EC, 0x800, 0, 0, 2, 0 },
    { 0x8F, 0, 0, -0x27F6, 0, 0x1388, 0x800, 0, 4, 0x12, 0 },
    { AREA_PLACEMENT_END },
};

static AreaObjectPlace D_map_akropolis_8017BE1C[6] = {
    { 1, 0xA0, 0, 3 },
    { 0xA, 2, 0, 1 },
    { 0x17, 2, 0, 1 },
    { 0x1B, 1, 0, 1 },
    { 0x2E, 0x801, 0, 0x101, 0x18D9, -0xE74, 0x95C },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BE7C[2] = {
    { 2, 0x101, 0, 0x201 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BE9C[5] = {
    { 3, 0x102, 0, 1 },
    { 4, 0x107, 0, 1, -0x96A, -0x3F2, -0x8FC },
    { 9, 4, 0, 1, 0x654, -0x672, 0xAC8 },
    { 0x16, 1, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BEEC[1] = {
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BEFC[2] = {
    { 0xB, 0x104, 0, 1, 0x410, -0x960, -0x4D8 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BF1C[4] = {
    { 0xC, 0x3C, 0, 1 },
    { 0xD, 0x8A, 0, 1 },
    { 0xE, 7, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BF5C[2] = {
    { 0x18, 8, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BF7C[2] = {
    { 0x10, 0x65, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BF9C[3] = {
    { 0x12, 0xA9, 0, 1 },
    { 0x15, 0x9D, 0, 1 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BFCC[2] = {
    { 0x1C, 0x103, 0, 1, -0x19C8, -0xA, -0x1F86, 0xFCE0 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017BFEC[2] = {
    { 0x13, 0x105, 0, 1, -0x1257, 0, -0x8FB, 0xFCE0 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017C00C[2] = {
    { 0x2F, 0x802, 0, 0x101, -0xC60, -0xC60, 0x9E0 },
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017C02C[1] = {
    { 0xFFFF },
};

static AreaObjectPlace D_map_akropolis_8017C03C[10] = {
    { 0x2D, 0x803, 0, 0x101, -0xC60, -0xC60, 0x9E0 },
    { 0x1E, 0x703, 0, 1 },
    { 0x1A, 3, 0, 1 },
    { 0x1F, 3, 0, 1 },
    { 0x20, 0xA1, 0, 1 },
    { 0x21, 0x3D, 0, 1 },
    { 0x22, 0x3F, 0, 1 },
    { 0x23, 0xB, 0, 1 },
    { 0x24, 0x6C, 0, 1 },
    { 0xFFFF },
};

InventoryBattleReward D_map_akropolis_8017C0DC[12] = {
    { GAME_LOCATION_KEY(1, 3, 2, 0), { 0x3B, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 3, 4, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(1, 4, 1, 0), { 2, 0, 0, 0xE } },
    { GAME_LOCATION_KEY(1, 8, 2, 0), { 0xA9, 0, 0, 0xAA } },
    { GAME_LOCATION_KEY(1, 9, 3, 0), { 8, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 9, 4, 0), { 1, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 10, 2, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(1, 10, 3, 0), { 0xA1, 0, 0, 0xA2 } },
    { GAME_LOCATION_KEY(1, 11, 1, 0), { 1, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 13, 1, 0), { 6, 0, 0, 1 } },
    { GAME_LOCATION_KEY(1, 15, 1, 0), { 2, 0, 0, 6 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

InventoryBattleReward D_map_akropolis_8017C16C[6] = {
    { GAME_LOCATION_KEY(1, 4, 1, 0), { 0x3B, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 8, 7, 0), { 0xAB, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 9, 7, 0), { 0x41, 0xA2, 0x3A, 0 } },
    { GAME_LOCATION_KEY(1, 10, 7, 0), { 6, 0, 0, 0 } },
    { GAME_LOCATION_KEY(1, 11, 7, 0), { 0x83, 0xA2, 0x3C, 0 } },
    { INVENTORY_BATTLE_REWARD_LIST_END },
};

StageMusicEntry D_map_akropolis_8017C1B4[168] = {
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x21, 0 },
    { 0x22, 0 },
    { 0x22, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x22, 0 },
    { 0x22, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x1D, 1 },
    { 0x22, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 5, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 6, 1 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0x1F, 1 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x22, 0 },
    { 0x22, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x23, 0 },
    { 0x24, 0 },
    { 0x24, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x24, 1 },
    { 0x24, 0 },
    { 0x24, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x24, 0 },
    { 0xFF, 0 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, 3 },
    { 0x26, 1 },
    { 0x26, 3 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x26, 1 },
    { 0xC, 1 },
    { 0x26, 0 },
    { 0xFF, 0 },
    { 0x20, 1 },
    { 0x22, 0 },
    { 0x22, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x25, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x11, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x11, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
};

StageMusicEntry D_map_akropolis_8017C304[20] = {
    { 0x1E, 2 },
    { 0xB, 2 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0xFF, 0 },
    { 0x45, 2 },
    { 0x46, 2 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};

/// The key-item panel heading. Three bytes follow the terminator that
/// nothing has been shown to read; they are reproduced so the block matches.
static const char D_map_akropolis_8017997C[12] = "Key Item\0L#\6";
