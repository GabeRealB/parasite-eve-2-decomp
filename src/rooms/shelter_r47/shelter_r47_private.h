#ifndef SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H
#define SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H

#include "common.h"

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

#include "rooms/room.h"

extern Task* gRoomCutsceneSoundTask;

/// Hotspot tables of the map terminal. Spawn argument `SHELTER_R47_MAP_MODE_TOUR`
/// selects the second, which carries only the previous and next pages.
extern ActionPromptHotspot D_shelter_r47_8018739C[];
extern ActionPromptHotspot D_shelter_r47_801873D8[];
/// Room views of the map pages, indexed by `ShelterR47MapTerminalWork::page`.
extern u8 D_shelter_r47_801873FC[];

/// Console layout during ordinary use or while changing the selected view.
enum {
    SHELTER_R47_CONSOLE_LAYOUT_CURRENT       = 0,
    SHELTER_R47_CONSOLE_LAYOUT_CHANGING_VIEW = 1,
};

/// Work block of the room's control console: the screen on which the player
/// selects one of five rows and flips its switch, each switch a game flag.
///
/// The task family's state table is `D_shelter_r47_8017D6C8`; the block is
/// allocated zeroed by its state 0 and stored at `Task::work`. Positions are
/// screen pixels relative to the display centre, and each one eases a quarter
/// of the way toward its target every frame.
typedef struct {
    s16 rowX[5];        // x of each row's switch sprite: 0x78 in the list, fanned out except for the selected row while the view changes
    u8  pad_A[2];
    s16 rowY[5];        // y of each row's switch sprite
    u8  pad_16[2];
    s16 toggles[5];     // working copies of the five console switches (0 off, 1 on), written back to their game flags on exit
    u8  pad_22[2];
    s16 labelX;         // x of the selected row's name sprite
    s16 labelY;         // y of the selected row's name sprite
    s16 headerX;        // x of the top-left header sprite, which slides in from the left
    s16 headerY;        // y of the top-left header sprite
    s16 buttonX;        // x of the switch button that flips the selected row
    s16 buttonY;        // y of the switch button; it slides off the bottom while the view changes
    s16 messageX;       // x of the status message sprite
    s16 messageY;       // y of the status message and its reveal cursor; slides off the bottom with the button
    s16 selection;      // `ActionPromptHotspot::id` of the confirmed hotspot: high byte the action kind (0 select row, 1 flip switch, 2..4 cap events), low byte its row
    u16 fade;           // fade-to-black level on exit: +0x10 a frame, clamped at 0xFF
    u8  pad_38[2];
    s16 wipeRed;        // red of the subtractive circular wipe shown on opening and while the view changes (0 clear .. 0xFF black)
    s16 wipeGreen;      // green of that wipe
    s16 wipeBlue;       // blue of that wipe
    s16 wipeGrey;       // grey level of the wipe's other vertices; reaching 0 or 0xFF ends the wipe
    s16 buttonFlash;    // frames left showing the pressed switch button; input resumes once it is 0
    s16 status;         // status message shown: 2 * row + that row's switch, indexing the message's reveal stops and cap events
    s16 backdropScroll; // horizontal scroll of the backdrop drawn in view 0x14, 0..0x140
    s16 revealPos;      // index into the status message's reveal stops; the message is revealed up to that stop
    s8  promptKind;     // `ActionPromptHotspot::promptKind` of the confirmed hotspot
    u8  pad_4B[3];
    u8  savedView;      // room view on entry, restored when the player dismisses the console
    s8  row;            // selected row, 0..4
    s8  previousRow;    // row selected before `row`, whose status stays shown while the view changes
    s8  guideStep;      // guided sequence (0 free use; 1..3 row the player must pick next; 4 done, so the console closes)
    s8  backdropToggle; // mirror of switch 3: bit 0 scrolls the backdrop toward 0, clear toward 0x140
    u8  pad_53;
} ShelterR47ConsoleWork;
STATIC_ASSERT_SIZEOF(ShelterR47ConsoleWork, 0x54);

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

/// Map page shown by the terminal, in previous/next order.
///
/// The marker tables, the page labels and the room view all follow this index.
enum {
    SHELTER_R47_MAP_PAGE_B1      = 0, // shelter basement 1
    SHELTER_R47_MAP_PAGE_B2      = 1, // shelter basement 2
    SHELTER_R47_MAP_PAGE_B3      = 2, // shelter basement 3
    SHELTER_R47_MAP_PAGE_NEO_ARK = 3, // Neo Ark
    SHELTER_R47_MAP_PAGE_1F      = 4, // shelter first floor
    SHELTER_R47_MAP_PAGE_COUNT   = 5,
};

/// How the terminal was opened. Copied from the task's spawn argument;
/// `SHELTER_R47_MAP_MODE_TOUR_DONE` is stored later, once the tour shows Neo Ark.
enum {
    SHELTER_R47_MAP_MODE_USE       = 0, // player at the terminal; cancel closes it and restores `savedView`
    SHELTER_R47_MAP_MODE_TIMED     = 1, // unattended viewing; fades in, holds, then fades out
    SHELTER_R47_MAP_MODE_TOUR      = 2, // event tour; previous page is refused, and only previous/next hotspots are installed
    SHELTER_R47_MAP_MODE_TOUR_DONE = 3, // Neo Ark has opened during the tour; the idle state closes the terminal
};

/// `hotspotId` of a confirmed terminal hotspot.
enum {
    SHELTER_R47_MAP_HOTSPOT_PANEL = 1, // quad left of the map
    SHELTER_R47_MAP_HOTSPOT_PREV  = 2, // previous page
    SHELTER_R47_MAP_HOTSPOT_NEXT  = 3, // next page
    SHELTER_R47_MAP_HOTSPOT_TITLE = 4, // page title
};

/// Open size of the map quad, in pixels. A width at or above
/// `SHELTER_R47_MAP_WIDTH_SETTLED` snaps to this size. While a page change
/// reopens the map, the side panel starts opening once the width reaches
/// `SHELTER_R47_MAP_WIDTH_PANEL`.
#define SHELTER_R47_MAP_WIDTH         0xE8
#define SHELTER_R47_MAP_HEIGHT        0xCE
#define SHELTER_R47_MAP_WIDTH_SETTLED 0xE5
#define SHELTER_R47_MAP_WIDTH_PANEL   0xB5

/// Open size of the panel left of the map, in pixels.
#define SHELTER_R47_MAP_PANEL_WIDTH  0x50
#define SHELTER_R47_MAP_PANEL_HEIGHT 0x60

/// On-screen x of the page labels, and the x they take while a page change
/// slides them off to the left. Pixels from the display centre.
#define SHELTER_R47_MAP_LABEL_X      (-0x9C)
#define SHELTER_R47_MAP_LABEL_X_AWAY (-0x104)

/// Room view selected on entry. It is the B1 page's view.
#define SHELTER_R47_MAP_ENTRY_VIEW 0x25

/// Frames the timed viewing stays up before it closes, unless the event is skipped.
#define SHELTER_R47_MAP_SHOW_LIMIT 300

/// Frames from the start of player use until the screen-on sound.
#define SHELTER_R47_MAP_SCREEN_ON_DELAY 6

/// Work block of the room's map terminal, stored at `Task::work`.
///
/// The player steps through five floor pages. The map quad and the panel to
/// its left each ease a quarter of the way toward their targets every frame,
/// and the page labels slide on the same easing. `openMode` records whether
/// this opening is player use, a timed viewing or the event tour. Positions
/// are screen pixels from the display centre.
typedef struct {
    u8                   pad_0[4];          // no read or write in this overlay; role unproven
    ActionPromptHotspot* hotspots;          // table hit-tested against the action cursor
    u8                   pad_8[2];          // no read or write in this overlay; role unproven
    s16                  mapWidth;          // width of the map quad; eases toward `mapTargetWidth`
    s16                  mapHeight;         // height of the map quad; eases toward `mapTargetHeight`
    s16                  mapTargetWidth;    // `SHELTER_R47_MAP_WIDTH` when open, 0 while a page change closes it
    s16                  mapTargetHeight;   // `SHELTER_R47_MAP_HEIGHT` when open, 0 while a page change closes it
    s16                  panelWidth;        // width of the quad left of the map; eases toward `panelTargetWidth`
    s16                  panelHeight;       // height of that quad; eases toward `panelTargetHeight`
    s16                  panelTargetWidth;  // `SHELTER_R47_MAP_PANEL_WIDTH` when open, 0 while a page change closes it
    s16                  panelTargetHeight; // `SHELTER_R47_MAP_PANEL_HEIGHT` when open, 0 while a page change closes it
    s16                  hotspotId;         // confirmed hotspot (`SHELTER_R47_MAP_HOTSPOT_PANEL` .. `SHELTER_R47_MAP_HOTSPOT_TITLE`)
    s16                  page;              // page shown (0 B1, 1 B2, 2 B3, 3 Neo Ark, 4 Shelter 1F); wraps over that range
    s16                  labelTargetX;      // x the page labels ease toward, on screen or off to the left during a page change
    s16                  labelX;            // x of the page-label sprites; eases a quarter of the way toward `labelTargetX` a frame
    u16                  fade;              // subtractive fade, 0 clear .. 0xFF black; closing adds 0x10 a frame, opening subtracts 8
    s16                  showFrames;        // frames the timed viewing has been up; past `SHELTER_R47_MAP_SHOW_LIMIT`, or on skip, it closes
    u16                  openFrames;        // frames the map quad has been fully open, zeroed while it grows; its low byte times four is the marker brightness
    s8                   promptKind;        // `ActionPromptHotspot::promptKind` of the confirmed hotspot
    u8                   savedView;         // room view on entry, restored when the player dismisses the terminal
    s8                   openMode;          // how the terminal was opened (`SHELTER_R47_MAP_MODE_USE` .. `SHELTER_R47_MAP_MODE_TOUR_DONE`)
    s8                   holdPrompt;        // 1 while the map quad is short of full width; the idle state hides the prompt until it is 0
    s8                   screenOnDelay;     // frames until the screen-on sound; set when player use begins, and the sound plays as it reaches 0
    u8                   pad_2D[3];         // tail padding to the block's 4-byte alignment
} ShelterR47MapTerminalWork;
STATIC_ASSERT_SIZEOF(ShelterR47MapTerminalWork, 0x30);

/// Task spawned by the room's cap script; polled and cleared by
/// `_shelterR47RestoreActorsAfterTerminalTask`.
extern Task* D_shelter_r47_8018A690;

extern u8 D_shelter_r47_8018A694;

extern u8 D_shelter_r47_8018A695;

/// Set the first time the map terminal's previous-page hotspot plays its one-off cap event.
extern u8 D_shelter_r47_8018A696;

/// Set the first time the map terminal's next-page hotspot plays its one-off cap event.
extern u8 D_shelter_r47_8018A697;

extern RoomCutsceneRec D_shelter_r47_8018A698;

extern u8 D_shelter_r47_80186FAC[5];

extern u8* D_shelter_r47_80187374[10];

/// Steps and draws the console's subtractive circular wipe toward clear.
///
/// `task->work` must be a live `ShelterR47ConsoleWork`, with each wipe component
/// in 0..255. Clears green, blue, red and finally grey in overlapping 32-level
/// steps. Returns 1 only on the frame grey crosses below zero and is clamped;
/// an already clear wipe returns 0. The RGB centre fades toward the grey rim.
/// Requires space for 32 `POLY_G3` and 32 `DR_MODE` packets in the frame arena;
/// links them into OT slot 11 for subtractive blending until GPU completion.
s16 shelterR47ConsoleClearWipe(Task* task);

/// Steps and draws the console's subtractive circular wipe toward black.
///
/// Uses the same work, component range and frame-arena contract as
/// `shelterR47ConsoleClearWipe`. Fills red, blue, green and finally grey in
/// overlapping 32-level steps. Returns 1 only on the frame grey reaches 256
/// and is clamped to 255; an already filled wipe returns 0. The grey centre
/// fades toward the RGB rim.
s16 shelterR47ConsoleFillWipe(Task* task);

/// Draws one composite control-console sprite at a screen-pixel origin.
///
/// `spriteId` must be 0..20: 0 header, 1..2 switch button, 3..12 status messages,
/// 13..17 row labels, 18..19 row switch and 20 selection/reveal marker.
/// `originX` and `originY` are relative to the display centre. Button sprites
/// 1 and 2 are suppressed in view 18, where console switch 2 is off.
/// The room's piece lists and textures must remain loaded. Requires a current
/// OT and frame arena with one `POLY_FT4` per piece; packets in slot 10 remain
/// borrowed by the GPU until the frame completes. No sprite bounds are checked.
void shelterR47ConsoleDrawSprite(s16 originX, s16 originY, s16 spriteId);

/// Opens the control console and initializes its owned display state.
///
/// State 0 allocates the full zeroed console work; failure kills the task.
/// Saves the live view, selects view 16, clears hotspot hits and starts a
/// separately tracked prompt task. Acquires the menu hold and holds gameplay
/// with the HUD hidden. Switch 3 selects backdrop scroll 0 or 320 pixels.
/// Only spawn argument 1 enables the guided sequence; other values use free
/// selection. Teardown restores the saved view, releases the hold/work and
/// kills the prompt task. Requires the loaded console tables and textures.
void shelterR47ConsoleInitializeTask(Task* task);

/// Draws the control console and accepts its first confirmed cursor hotspot.
///
/// Requires live `ShelterR47ConsoleWork`, the loaded console hotspot/texture
/// tables and a current frame arena/OT. CAP playback holds input. A confirmed
/// hotspot stores its ID and prompt kind, hides the cursor and enters state 4.
/// Guide step 4 enters fade-out state 12; cancel in free use enters state 6.
/// Hotspot rectangles and cursor coordinates use display-centred pixels.
void shelterR47ConsoleSelectHotspotTask(Task* task);

/// Advances the control-console presentation and queues its sprites for this frame.
///
/// Requires live `ShelterR47ConsoleWork`, loaded console textures and a current
/// frame arena/OT. `changingView` is zero for the current row; any nonzero value
/// moves the button and status below the screen, retains the previous row's
/// status and fans out the other rows. Coordinates are display-centred pixels;
/// easing uses signed quarter steps. In view 20 the backdrop scrolls by one
/// pixel toward 0 or 320. Decrements `buttonFlash` and advances the status reveal
/// before selecting this frame's status. GPU packets borrow the arena until
/// frame completion; the row and previous-row indices must be 0..4.
void shelterR47ConsoleUpdateAndDraw(Task* task, s16 changingView);

/// Loads the console's five working switches from their saved game-flag nibbles.
///
/// Requires live `ShelterR47ConsoleWork`. Copies the full 0..15 nibble values
/// into the five signed-halfword slots in row order. The low bit of switch 2
/// selects its off/on room image, view 18/36, for later row changes. This does
/// not change the current view or commit any saved flags.
void shelterR47ConsoleLoadSwitches(Task* task);

/// Marks every console hotspot containing a display-centred pixel position.
///
/// Requires live `ShelterR47ConsoleWork` and a writable table ending with
/// `ACTION_PROMPT_HOTSPOT_END`. Rectangle edges are inclusive; all entries are
/// updated, so overlapping entries can be hit together. The switch-button
/// hotspot is suppressed for row 1. Returns 1 if any eligible entry is hit,
/// otherwise 0; the sentinel is neither read as a rectangle nor modified.
s32 shelterR47ConsoleHitTestHotspots(Task* task, ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);

/// Rearms the console and map terminal's four first-use CAP events.
///
/// Called on room entry. Clears the switch-button, row-selection, previous-page
/// and next-page event latches for subsequent terminal use; saved progress and
/// guided-tour state are retained.
void shelterR47ResetTerminalFirstUseEvents(void);

/// Eases the map and side-panel sizes and queues their textured quads.
///
/// Borrows initialized terminal work with page 0..4 and pixel dimensions.
/// Each size moves one quarter of the signed difference toward its target.
/// A map width at least 229 snaps both sizes to 232x206, draws the page overlay,
/// clears the prompt hold and advances the open-frame counter. Below that width
/// it hides the prompt and resets the counter. Always draws the map and panel
/// backgrounds, using OT entries 11/12 and one packet each plus page contents.
/// Textures, work and packet storage must remain live through GPU completion.
void shelterR47MapTerminalUpdateAndDrawQuads(Task* task);

/// Queues the map terminal's previous-page button at (-150, 63) screen pixels.
///
/// Requires the terminal textures and room overlay to remain loaded, a current
/// OT and one `SpriteDrawModePacket` of arena space. Uses raw semitransparent
/// texture in OT slot 11; the packet remains borrowed until GPU completion.
void shelterR47MapTerminalDrawPreviousButton(void);

/// Queues the map terminal's next-page button at (-144, 80) screen pixels.
///
/// Uses the arena, texture lifetime and OT contract of
/// `shelterR47MapTerminalDrawPreviousButton` with the next-button palette.
void shelterR47MapTerminalDrawNextButton(void);

/// Eases the terminal labels' X position and queues the selected page's title.
///
/// Requires live `ShelterR47MapTerminalWork`; `page` is one of the five
/// `SHELTER_R47_MAP_PAGE_*` indices. Label coordinates are display-centred
/// pixels, stepped a signed quarter of the remaining distance. Uses the arena
/// and texture lifetime of `shelterR47MapTerminalDrawPreviousButton`.
void shelterR47MapTerminalDrawPageTitle(Task* task, s16 page);

/// Queues the selected map page's two caption strips at the current label X.
///
/// Requires live `ShelterR47MapTerminalWork` and a valid
/// `SHELTER_R47_MAP_PAGE_*` index. Call after
/// `shelterR47MapTerminalDrawPageTitle` to use this frame's eased position.
/// Uses its arena and texture lifetime contract, reserving two packets.
void shelterR47MapTerminalDrawPageCaptions(Task* task, s16 page);

/// Runs the control console's action-cursor task.
///
/// `task->state` must be 0 (reset both ports' prompts and advance to 1) or
/// 1 (move, classify presses and draw the selected ports' cursors).
/// Spawn argument 1 selects port 0 with 1, port 1 with 2, and both otherwise.
/// Borrows the gameplay-owned prompts, pad samples, cursor textures and current
/// frame arena/OT. The task needs no work allocation; its console owner kills it.
void shelterR47ConsolePromptTask(Task* task);

/// Runs one state of the room's control-console screen, including its teardown.
///
/// `task->state` must be 0..13. State 0 allocates owned console work and spawns
/// the action-cursor task; later states require live console work. Spawn
/// argument 1 selects the guided sequence with 1 and free use otherwise.
/// Input states change rows or switches; exits save the switches and release
/// control, display holds and tasks. The overlay and console textures must stay
/// loaded through the dispatched callback, which may release the task.
void shelterR47ConsoleTask(Task* task);

#endif // SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H
