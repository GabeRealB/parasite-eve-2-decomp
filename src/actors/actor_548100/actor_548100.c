#include "actors/actor_548100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room/action_prompt.h"

/// Indices of the mine power-panel task callbacks.
enum {
    ACTOR_548100_STATE_INIT_PANEL              = 0,
    ACTOR_548100_STATE_ARM_CURSOR              = 1,
    ACTOR_548100_STATE_SCAN_HOTSPOTS           = 2,
    ACTOR_548100_STATE_OPEN_COMMANDS           = 3,
    ACTOR_548100_STATE_HANDLE_CHOICE           = 4,
    ACTOR_548100_STATE_EXIT                    = 5,
    ACTOR_548100_STATE_PLACE_BATTERY           = 6,
    ACTOR_548100_STATE_WAIT_BATTERY_RETURN     = 7,
    ACTOR_548100_STATE_WAIT_SWITCH_CAPTION     = 8,
    ACTOR_548100_STATE_ANIMATE_FLOW            = 9,
    ACTOR_548100_STATE_WAIT_COMPLETION_CAPTION = 10,
};

/// Socket contents and CAP command-6 variants used for battery placement/removal.
enum {
    ACTOR_548100_SOCKET_EMPTY             = 0,
    ACTOR_548100_SOCKET_FIRST_BATTERY     = 1,
    ACTOR_548100_SOCKET_SECOND_BATTERY    = 2,
    ACTOR_548100_CAP_COMMAND_SOCKET       = 6,
    ACTOR_548100_CAP_SOCKET_EMPTY         = 0,
    ACTOR_548100_CAP_SOCKET_POWERED       = 1,
    ACTOR_548100_CAP_SOCKET_FIRST_PICKUP  = 2,
    ACTOR_548100_CAP_SOCKET_LOCKED        = 3,
    ACTOR_548100_CAP_SOCKET_OCCUPIED      = 4,
    ACTOR_548100_CAP_SOCKET_SECOND_PICKUP = 5,
};

/// Battery item ids and the switch caption's completion keys.
enum {
    ACTOR_548100_BATTERY_FIRST_ITEM  = 0x120,
    ACTOR_548100_BATTERY_SECOND_ITEM = 0x12C,
    ACTOR_548100_CAPTION_SWITCH_ON   = 0xB,
    ACTOR_548100_CAPTION_SWITCH_OFF  = 0x15,
};

/// Authored socket, wiring and route domains of the panel's diagram.
enum {
    ACTOR_548100_SOCKET_COUNT    = 4,
    ACTOR_548100_WIRING_FIRST    = 1,
    ACTOR_548100_WIRING_SECOND   = 2,
    ACTOR_548100_CIRCUIT_END     = -1,
    ACTOR_548100_ROUTE_UNUSED    = 0,
    ACTOR_548100_ROUTE_END       = 0,
    ACTOR_548100_ROUTE_GAP       = 0xFF,
    ACTOR_548100_STOP_NODE_NONE  = 0,
    ACTOR_548100_NODE_ID_MASK    = 0xFF,
    ACTOR_548100_NODE_KEY_STRIDE = 100,
};

/// Route-table ids, preserving the socket order within each authored wiring.
enum {
    ACTOR_548100_ROUTE_FIRST_SOCKET_1  = 1,
    ACTOR_548100_ROUTE_FIRST_SOCKET_2  = 2,
    ACTOR_548100_ROUTE_FIRST_SOCKET_3  = 3,
    ACTOR_548100_ROUTE_FIRST_SOCKET_4  = 4,
    ACTOR_548100_ROUTE_SECOND_SOCKET_1 = 5,
    ACTOR_548100_ROUTE_SECOND_SOCKET_2 = 6,
    ACTOR_548100_ROUTE_SECOND_SOCKET_3 = 7,
    ACTOR_548100_ROUTE_SECOND_SOCKET_4 = 8,
    ACTOR_548100_ROUTE_MAIN_FEED       = 9,
    ACTOR_548100_ROUTE_COUNT           = 10,
};

/// Output choices and the changed-picture rectangle after the four socket pieces.
enum {
    ACTOR_548100_OUTPUT_DOOR    = 1,
    ACTOR_548100_OUTPUT_PASSAGE = 2,
    ACTOR_548100_PIECE_SWITCH   = 4,
};

/// Panel packet depths and pixel coordinates; wires use a two-pixel origin bias.
enum {
    ACTOR_548100_OT_WIRES           = 0x3FC,
    ACTOR_548100_OT_PANEL_PIECES    = 0x3FE,
    ACTOR_548100_WIRE_ORIGIN_X      = 158,
    ACTOR_548100_WIRE_ORIGIN_Y      = 118,
    ACTOR_548100_SCREEN_CENTER_X    = 160,
    ACTOR_548100_SCREEN_CENTER_Y    = 120,
    ACTOR_548100_PANEL_TEXTURE_PAGE = 0x116, // 15-bit texels at VRAM (384, 256)
};

/// Pulse level at which a wire colour equals its authored RGB bytes.
enum { ACTOR_548100_COLOR_PULSE_ONE = 32 };

/// Hotspot choices shared by the panel rectangles and their command handler.
enum {
    ACTOR_548100_CHOICE_SOCKET_1    = 1,
    ACTOR_548100_CHOICE_SOCKET_2    = 2,
    ACTOR_548100_CHOICE_SOCKET_3    = 3,
    ACTOR_548100_CHOICE_SOCKET_4    = 4,
    ACTOR_548100_CHOICE_SWITCH      = 5,
    ACTOR_548100_CHOICE_POWER_LIGHT = 6,
    ACTOR_548100_CHOICE_POWER_LINES = 7,
    ACTOR_548100_CHOICE_GROUND_WIRE = 8,
    ACTOR_548100_CHOICE_GATE_LOCKS  = 9,
};

/// Pulse phases stored in a byte.
enum {
    ACTOR_548100_PULSE_RISING  = 0,
    ACTOR_548100_PULSE_FALLING = 1,
};

/// Publishes a route node, skipping a gap marker when the caller already selected that branch.
///
/// `cursor` must be a `const u8*` local and `previousNode` a distinct `u8` local;
/// `isGap` is a side-effect-free flag (0 node, nonzero gap). Use this compound
/// statement inside a braced branch. The cursor is evaluated again after a gap
/// increment; the node is reread after preceding wire-state stores. A gap marker
/// must be followed by a valid node byte in the same route. This selects the next
/// disconnected segment's start without adding an edge; the caller owns the
/// ordinary advance and end/stop tests. No caller locals are captured and the
/// macro does not validate the flag against the current byte.
#define ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, isGap) \
    {                                                             \
        if ((isGap) != 0) {                                       \
            (cursor)++;                                           \
        }                                                         \
        (previousNode) = *(cursor);                               \
    }

/// Maps a battery socket number in 1..4 to its saved contents flag.
///
/// Evaluates socket once and returns the flag index, without reading its value.
/// The consecutive flags end at `GAME_FLAG_MINE_POWER_PANEL_SOCKET_4`; their
/// nibbles hold `ACTOR_548100_SOCKET_EMPTY`, `_FIRST_BATTERY` or `_SECOND_BATTERY`.
/// Panel hotspot choices use the same one-based socket domain.
#define ACTOR_548100_SOCKET_FLAG(socket) (GAME_FLAG_MINE_POWER_PANEL_SOCKET_4 - ACTOR_548100_SOCKET_COUNT + (socket))

/// One stretch of current in an `_Actor548100Circuit`: the route it runs along
/// and how far along it the current gets.
///
/// A route is a string of the diagram's node ids, picked from the panel's
/// routes by its id. Current that nothing obstructs runs the whole route.
/// Where two batteries' routes cross, both legs name the crossing as their
/// stop: the current runs only that far, and once it has arrived the wires up
/// to there are drawn as a short circuit instead of as carrying current.
typedef struct {
    u8 route;    // route id; 0 when the leg is unused
    u8 stopNode; // node id the current stops at, where the route crosses another battery's; 0 when it runs the whole route
} _Actor548100Leg;
STATIC_ASSERT_SIZEOF(_Actor548100Leg, 0x2);

/// Work block of the task that runs the mine power panel screen, allocated by
/// its first state and kept at `Task::work`.
///
/// The panel is a circuit diagram with four battery sockets and a switch. The
/// idle state latches the hotspot the player confirms in `choice` and
/// `promptKind`, and the states after it open the command prompt for that
/// hotspot. Accepting the command carries the choice out: an occupied socket
/// offers its battery back as the room pickup `pickupObject` and is emptied if
/// the player takes it, the switch is thrown, and the other hotspots only play
/// a caption. Using a battery key item from the item menu while a socket is
/// latched leaves the item in `usedItem` instead, and the socket takes it once
/// the menu has closed.
///
/// Throwing the switch plays current flowing out along the legs of the
/// `_Actor548100Circuit` that matches the filled sockets. Each leg's route is
/// measured once, and the long and third progress distances then grow by
/// `ACTOR_548100_FLOW_SPEED` per tick until every leg has run its length. The
/// two battery legs are kept ordered by length rather than by socket, and the
/// shorter one's distance is scaled from the longer one's, so both arrive on
/// the same frame. An unused leg measures 1, which keeps that scaling defined
/// and finishes the leg on the first frame.
typedef struct {
    u8              unknown_0[2];  // Never read or written by the actor; role unproven
    s16             choice;        // `ActionPromptHotspot::id` of the confirmed hotspot (0 none, 1-4 the battery sockets, 5 the switch, 6-9 the caption-only parts of the panel)
    s16             usedItem;      // Battery key item (0x120 or 0x12C) used from the item menu on the socket in `choice` and not yet placed; 0 none
    s8              promptKind;    // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    s8              pickupObject;  // Id, in the stage's two-bit object states, of the pickup that hands back the chosen socket's battery (4 for the battery a socket flag records as 1, 5 for 2)
    s16             longLength;    // Length of the longer battery leg's route, in `_Actor548100Edge::length` units; 1 when that leg is unused
    s16             longProgress;  // Distance the current has run along that route, 0..`longLength`
    s16             shortLength;   // Length of the shorter battery leg's route; 1 when that leg is unused
    s16             shortProgress; // Distance the current has run along that route: `longProgress` scaled by `shortLength / longLength`
    s16             thirdLength;   // Length of the circuit's third leg, the same in every circuit of a wiring; 1 when it is unused
    s16             thirdProgress; // Distance the current has run along that leg, 0..`thirdLength`
    _Actor548100Leg longLeg;       // The circuit's longer battery leg
    _Actor548100Leg shortLeg;      // The circuit's shorter battery leg
} _Actor548100Work;
STATIC_ASSERT_SIZEOF(_Actor548100Work, 0x18);

/// What one arrangement of batteries in the panel's four sockets does once the
/// switch is thrown: the legs the current runs along and the outputs it
/// powers.
///
/// Each of the panel's two wirings has a table of these, one record for every
/// arrangement of at most two batteries, and the record whose sockets are
/// exactly the filled ones is the panel's circuit. `leg[0]` and `leg[1]` carry
/// the current of the batteries in `socketA` and `socketB`, and `leg[2]` is a
/// feed no battery supplies, the same in every record of a wiring. An
/// arrangement whose two routes cross is a short circuit: both battery legs
/// stop at the crossing, and a leg that stops short lights neither output.
typedef struct {
    s8              socketA;       // socket (1-4) of the first battery; 0 when no socket is filled; -1 in the record that ends the first wiring's table
    s8              socketB;       // socket of the second battery; 0 when fewer than two are filled
    _Actor548100Leg leg[3];        // 0 and 1: current of the batteries in `socketA` and `socketB`; 2: the feed no battery supplies
    u8              powersDoor;    // nonzero when the circuit powers the panel's first output, the gorge's door to the cavern
    u8              powersPassage; // nonzero when it powers the second output, the cavern's secret passage
} _Actor548100Circuit;
STATIC_ASSERT_SIZEOF(_Actor548100Circuit, 0xA);

/// Values of `_Actor548100Edge::layout`: which of the panel's two wirings carry
/// the wire.
///
/// The panel is drawn in its first wiring until `GAME_FLAG_MINE_POWER_PANEL_STAGE`
/// reaches 2 and in its second from then on; a wire the current wiring lacks is
/// `ACTOR_548100_EDGE_STATE_ABSENT`.
enum {
    ACTOR_548100_EDGE_LAYOUT_BOTH        = 0, // part of both wirings
    ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY = 1, // only in the second wiring
    ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY  = 2  // only in the first wiring
};

/// Values of `_Actor548100Edge::state`: how the wire is drawn this frame.
///
/// `STOP`, `FLOW` and `SHORT` each select one of the panel's three pulsing
/// colours, named as the actor's retained colour-editor rows caption them.
enum {
    ACTOR_548100_EDGE_STATE_ABSENT      = 0, // not part of the current wiring: not drawn
    ACTOR_548100_EDGE_STATE_STOP        = 1, // no current
    ACTOR_548100_EDGE_STATE_FLOW        = 2, // carrying current
    ACTOR_548100_EDGE_STATE_SHORT       = 3, // on a route that stops at a node where it meets another battery's
    ACTOR_548100_EDGE_STATE_FLOW_FROM_A = 4, // current has entered at `nodeA` and reached `flowLength`: `FLOW` up to there, `STOP` beyond
    ACTOR_548100_EDGE_STATE_FLOW_FROM_B = 5  // the same, entered at `nodeB`
};

/// One wire of the mine power panel's circuit diagram: a straight segment
/// between two of the diagram's nodes.
///
/// The diagram is a graph. A node id indexes the node point table, and a route
/// of current is a string of node ids; each consecutive pair of a route is
/// looked up in a node-pair matrix that holds the index of the wire joining
/// them, in either direction. A record whose `nodeA` is 0 ends the wire table.
///
/// Only the two nodes and `layout` are authored. The geometry fields are
/// derived from the node points when the panel opens, and `state` and
/// `flowLength` are rewritten every frame: each wire is first reset to `STOP`
/// or `ABSENT` for the current wiring, and the routes the placed batteries
/// feed are then walked over that.
typedef struct {
    u8  nodeA;      // node id of one end; 0 in the record that ends the table
    u8  nodeB;      // node id of the other end
    u8  layout;     // `ACTOR_548100_EDGE_LAYOUT_*`
    u8  vertical;   // axis the wire spans further (0 x, 1 y)
    s16 coordA;     // `nodeA`'s drawn coordinate on that axis; stored when the panel opens and not read
    s16 coordB;     // `nodeB`'s coordinate on that axis, likewise
    u8  state;      // `ACTOR_548100_EDGE_STATE_*`
    s16 length;     // distance between the two nodes on that axis, less 2; what a route's length is summed from
    s16 flowLength; // in the `FLOW_FROM_*` states, how far along that axis the current has come from the node it entered at
} _Actor548100Edge;
STATIC_ASSERT_SIZEOF(_Actor548100Edge, 0xE);

/// One piece of the panel's picture that changes with its state: the battery
/// in a filled socket, or the side of the panel that holds the thrown switch.
///
/// The changed picture is a texture page laid over the screen pixel for pixel,
/// so a single rectangle says both which texels to take and where to draw
/// them. Its corners are in 320x240 screen pixels from the top-left corner,
/// which are the texture coordinates as they stand and, less the screen centre
/// (160, 120), the quad's position.
typedef struct {
    s16 left;
    s16 top;
    s16 right;
    s16 bottom;
} _Actor548100TexRect;
STATIC_ASSERT_SIZEOF(_Actor548100TexRect, 0x8);

/// Label of one of the three wire colours beside pointers to its components.
///
/// A wire colour is three separately stored bytes: its red, green and blue at
/// the peak of the panel's pulse. Nothing in the actor reads a row. A label
/// paired with pointers to bytes the drawing code rereads every frame is the
/// shape of a tuning menu's rows, but that role is unproven.
typedef struct {
    u8*         rgb[3]; // the colour's red, green and blue bytes, in that order
    const char* label;  // the colour's name
} _Actor548100ColorRow;
STATIC_ASSERT_SIZEOF(_Actor548100ColorRow, 16);

static void _actionPromptResetDefault(Task* task);
static void _actor548100InitPanel(Task* task);
static void _actor548100ScanHotspots(Task* task);
static void _actor548100HandleHotspotChoice(Task* task);
static void _actor548100WaitSwitchCaption(Task* task);
static void _actor548100SelectCircuit(void);
static void _actor548100InitWireGraph(void);
static s32  _actionPromptHitTestFirstChoice(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
static s32  _actor548100MeasureLeg(s32 routeId, u8 stopNode);
static void _actor548100ArmCursor(Task* task);
static void _actor548100OpenHotspotCommands(Task* task);
static void _actor548100ExitPanel(Task* task);
static void _actor548100PlaceQueuedBattery(Task* task);
static void _actor548100WaitBatteryReturn(Task* task);
static void _actor548100AnimateCircuitFlow(Task* task);
static void _actor548100WaitCompletionCaption(Task* task);
static void _actor548100DrawPanelPiece(const _Actor548100TexRect* rect);
static void _actor548100DrawOutputIndicator(s32 output);
static void _actor548100DrawWire(s32 nodeA, s32 nodeB, u8 red, u8 green, u8 blue);
static void _actor548100SetLegFlowProgress(s32 routeId, s32 stopNode, s16 progress);
static void _actor548100ScaleFlowColor(s16 pulse, s8* red, s8* green, s8* blue);
static void _actor548100ScaleStopColor(s16 pulse, s8* red, s8* green, s8* blue);
static void _actor548100ScaleShortColor(s16 pulse, s8* red, s8* green, s8* blue);
static void _actor548100SetLegComplete(s32 routeId, u8 stopNode);
static void _actor548100DrawWires(void);
static void _actor548100ResetWireStates(void);

static s32  _actor548100UseBattery(Task* task, s32 messageId, s32 itemId, s32 unused);
static void _actor548100PromptTask(Task* task);

static const char _gActor548100ShortWireLabel[6];
static const char _gActor548100StopWireLabel[5];
static const char _gActor548100FlowWireLabel[5];
static void       _actor548100PanelTask(Task* task);

/// Action-cursor child task recipe for the mine power panel.
///
/// The child has no automatic task body and runs at priority 192. The panel
/// retains its borrowed handle until requesting completion on exit; cursor work
/// and presentation are owned by the spawned prompt task.
static TaskDesc _gActor548100PanelPromptTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor548100PromptTask, { .value = 0 } };

/// Forwarded key-item-use requests handled by the mine power panel.
///
/// The selected battery ID is latched into live panel work. Replies use
/// ROOM_KEY_ITEM_USE_* to choose the item menu presentation; inventory changes
/// are deferred to panel interaction. The final end-ID/null row rejects other IDs.
static TaskMessageEntry _gActor548100PanelMessages[2] = {
    { ROOM_MESSAGE_USE_KEY_ITEM, _actor548100UseBattery },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Mutable diagram wires: 91 node pairs followed by a nodeA-zero terminator.
///
/// Endpoints and wiring membership are authored. Opening the panel derives
/// dominant-axis pixel geometry; each frame replaces the flow drawing state.
static _Actor548100Edge _gActor548100PanelWires[92] = {
    { 68, 1, ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY },
    { 1, 2, ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY },
    { 2, 73, ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY },
    { 73, 74, ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY },
    { 69, 75, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 3, 4, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 4, 5, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 5, 6, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 6, 7, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 7, 26, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 26, 30, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 30, 34, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 34, 54, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 54, 53, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 53, 52, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 52, 51, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 51, 57, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 57, 58, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 70, 76, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 76, 77, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 8, 9, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 9, 10, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 10, 11, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 11, 12, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 12, 13, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 13, 14, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 14, 25, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 25, 29, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 29, 33, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 33, 47, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 47, 46, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 46, 45, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 45, 44, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 44, 43, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 43, 50, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 50, 56, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 56, 59, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 59, 60, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 60, 61, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 61, 62, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 71, 78, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 15, 16, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 16, 17, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 17, 18, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 18, 19, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 19, 20, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 20, 28, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 28, 32, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 32, 42, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 42, 41, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 41, 40, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 40, 39, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 39, 38, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 38, 49, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 49, 55, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 55, 63, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 63, 64, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 64, 66, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 66, 67, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 72, 79, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 79, 80, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 21, 22, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 22, 23, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 23, 24, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 24, 27, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 27, 31, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 31, 37, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 37, 36, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 36, 35, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 35, 48, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 48, 65, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 65, 66, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 4, 9, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 5, 10, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 6, 13, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 25, 26, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 29, 30, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 34, 33, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 44, 53, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 50, 51, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 11, 17, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 12, 19, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 41, 46, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 40, 45, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 55, 56, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 16, 22, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 18, 23, ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY },
    { 27, 28, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 32, 31, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 36, 39, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 48, 49, ACTOR_548100_EDGE_LAYOUT_BOTH },
    { 0 },
};

/// First-wiring circuits for empty sockets, four singles and six pairs, then a terminator.
///
/// Each record gives the battery routes, crossing stops and powered outputs.
/// The third leg is always the independent main feed. SocketA -1 ends lookup.
static _Actor548100Circuit _gActor548100FirstWiringCircuits[12] = {
    { 0, 0, { { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 1, 0, { { ACTOR_548100_ROUTE_FIRST_SOCKET_1, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 2, 0, { { ACTOR_548100_ROUTE_FIRST_SOCKET_2, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 1, 1 },
    { 3, 0, { { ACTOR_548100_ROUTE_FIRST_SOCKET_3, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 4, 0, { { ACTOR_548100_ROUTE_FIRST_SOCKET_4, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 1, 2, { { ACTOR_548100_ROUTE_FIRST_SOCKET_1, 9 }, { ACTOR_548100_ROUTE_FIRST_SOCKET_2, 9 }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 1, 3, { { ACTOR_548100_ROUTE_FIRST_SOCKET_1, 11 }, { ACTOR_548100_ROUTE_FIRST_SOCKET_3, 11 }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 1, 4, { { ACTOR_548100_ROUTE_FIRST_SOCKET_1, 36 }, { ACTOR_548100_ROUTE_FIRST_SOCKET_4, 36 }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 2, 3, { { ACTOR_548100_ROUTE_FIRST_SOCKET_2, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_FIRST_SOCKET_3, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 1, 1 },
    { 2, 4, { { ACTOR_548100_ROUTE_FIRST_SOCKET_2, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_FIRST_SOCKET_4, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 1, 1 },
    { 3, 4, { { ACTOR_548100_ROUTE_FIRST_SOCKET_3, 27 }, { ACTOR_548100_ROUTE_FIRST_SOCKET_4, 27 }, { ACTOR_548100_ROUTE_MAIN_FEED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { ACTOR_548100_CIRCUIT_END, ACTOR_548100_CIRCUIT_END, { { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
};

/// Second-wiring circuits for empty sockets, four singles and six pairs, without a terminator.
///
/// Lookup requires at most two occupied sockets. Only sockets 1 and 4 together
/// power both outputs; the third leg is unused in every record.
static _Actor548100Circuit _gActor548100SecondWiringCircuits[11] = {
    { 0, 0, { { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 1, 0, { { ACTOR_548100_ROUTE_SECOND_SOCKET_1, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 1, 0 },
    { 2, 0, { { ACTOR_548100_ROUTE_SECOND_SOCKET_2, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 3, 0, { { ACTOR_548100_ROUTE_SECOND_SOCKET_3, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 4, 0, { { ACTOR_548100_ROUTE_SECOND_SOCKET_4, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 1 },
    { 1, 2, { { ACTOR_548100_ROUTE_SECOND_SOCKET_1, 9 }, { ACTOR_548100_ROUTE_SECOND_SOCKET_2, 9 }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 1, 3, { { ACTOR_548100_ROUTE_SECOND_SOCKET_1, 13 }, { ACTOR_548100_ROUTE_SECOND_SOCKET_3, 13 }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 1, 4, { { ACTOR_548100_ROUTE_SECOND_SOCKET_1, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_SECOND_SOCKET_4, ACTOR_548100_STOP_NODE_NONE }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 1, 1 },
    { 2, 3, { { ACTOR_548100_ROUTE_SECOND_SOCKET_2, 18 }, { ACTOR_548100_ROUTE_SECOND_SOCKET_3, 18 }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 2, 4, { { ACTOR_548100_ROUTE_SECOND_SOCKET_2, 11 }, { ACTOR_548100_ROUTE_SECOND_SOCKET_4, 11 }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
    { 3, 4, { { ACTOR_548100_ROUTE_SECOND_SOCKET_3, 22 }, { ACTOR_548100_ROUTE_SECOND_SOCKET_4, 22 }, { ACTOR_548100_ROUTE_UNUSED, ACTOR_548100_STOP_NODE_NONE } }, 0, 0 },
};

/// Changed-picture rectangles for four filled battery sockets and the thrown switch.
///
/// Entries 0..3 follow socket order; `ACTOR_548100_PIECE_SWITCH` is the final
/// entry. Opposite corners use pixel coordinates, shared by screen placement
/// and the 15-bit panel texture at VRAM (384, 256).
static _Actor548100TexRect _gActor548100PanelPieces[5] = {
    { 76, 33, 89, 47 },
    { 89, 45, 103, 59 },
    { 76, 57, 89, 71 },
    { 89, 69, 103, 83 },
    { 0, 0, 75, 239 },
};

/// Writable cursor hotspots in screen-centred pixels, ending at the id -1 sentinel.
///
/// Twelve inclusive rectangles select four sockets, the switch and caption
/// targets. Hit testing marks every overlap; confirmation uses the first hit.
/// Repeated power-line rectangles and their order are part of the authored table.
static ActionPromptHotspot _gActor548100PanelHotspots[13] = {
    { -84, -87, 14, 14, ACTOR_548100_CHOICE_SOCKET_1, 0, 0 },
    { -71, -75, 14, 14, ACTOR_548100_CHOICE_SOCKET_2, 0, 0 },
    { -84, -63, 14, 14, ACTOR_548100_CHOICE_SOCKET_3, 0, 0 },
    { -71, -51, 14, 14, ACTOR_548100_CHOICE_SOCKET_4, 0, 0 },
    { -134, 0, 43, 88, ACTOR_548100_CHOICE_SWITCH, 0, 0 },
    { -134, -98, 44, 200, ACTOR_548100_CHOICE_POWER_LIGHT, 0, 0 },
    { 63, 42, 44, 38, ACTOR_548100_CHOICE_GATE_LOCKS, 0, 0 },
    { -58, -96, 189, 55, ACTOR_548100_CHOICE_POWER_LINES, 0, 0 },
    { 75, -46, 56, 117, ACTOR_548100_CHOICE_POWER_LINES, 0, 0 },
    { 75, -46, 56, 117, ACTOR_548100_CHOICE_POWER_LINES, 0, 0 },
    { -79, -1, 152, 93, ACTOR_548100_CHOICE_POWER_LINES, 0, 0 },
    { -37, 92, 96, 20, ACTOR_548100_CHOICE_GROUND_WIRE, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Authored red channel of the flowing-current colour at pulse level 32.
static u8 _gActor548100FlowBaseRed = 16;

/// Authored green channel of the flowing-current colour at pulse level 32.
static u8 _gActor548100FlowBaseGreen = 165;

/// Authored blue channel of the flowing-current colour at pulse level 32.
static u8 _gActor548100FlowBaseBlue = 98;

/// Authored red channel of the stopped-current colour at pulse level 32.
static u8 _gActor548100StopBaseRed = 60;

/// Authored green channel of the stopped-current colour at pulse level 32.
static u8 _gActor548100StopBaseGreen = 60;

/// Authored blue channel of the stopped-current colour at pulse level 32.
static u8 _gActor548100StopBaseBlue = 16;

/// Authored red channel of the short-circuit colour at pulse level 32.
static u8 _gActor548100ShortBaseRed = 160;

/// Authored green channel of the short-circuit colour at pulse level 32.
static u8 _gActor548100ShortBaseGreen = 16;

/// Retained byte storage whose first byte is the short-circuit blue channel at pulse level 32.
///
/// Only element 0 is accessed. The role and membership of the following 27 zero
/// bytes are unproven; the stored span is preserved without treating them as padding.
static u8 _gActor548100ShortBaseBlueStorage[28] = {
    32,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Wire-color labels paired with writable base red, green and blue components.
///
/// Rows are Flow, Stop and Short, at pulse level 32. The drawing code reads the
/// same component bytes directly; the shipped actor has no reader of this table.
/// Its original UI or tuning purpose is unproven.
static _Actor548100ColorRow _gActor548100WireColorRows[3] = {
    { { &_gActor548100FlowBaseRed, &_gActor548100FlowBaseGreen, &_gActor548100FlowBaseBlue }, _gActor548100FlowWireLabel },
    { { &_gActor548100StopBaseRed, &_gActor548100StopBaseGreen, &_gActor548100StopBaseBlue }, _gActor548100StopWireLabel },
    { { &_gActor548100ShortBaseRed, &_gActor548100ShortBaseGreen, _gActor548100ShortBaseBlueStorage }, _gActor548100ShortWireLabel },
};

TaskDesc gActor548100PanelTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor548100PanelTask, { .value = 0 } };

/// Diagram node positions in pixels from the screen's top-left corner.
///
/// Wires index nodes 1..80; entry 0 is unused. The final (-1, -1) pair is
/// retained but no consumer uses it as a terminator. Drawing subtracts (158, 118).
static DVECTOR _gActor548100DiagramNodes[82] = {
    { 0, 0 },
    { 79, 26 },
    { 286, 26 },
    { 89, 38 },
    { 125, 38 },
    { 150, 38 },
    { 232, 38 },
    { 271, 38 },
    { 102, 51 },
    { 125, 51 },
    { 150, 51 },
    { 169, 51 },
    { 208, 51 },
    { 232, 51 },
    { 260, 51 },
    { 89, 62 },
    { 136, 62 },
    { 169, 62 },
    { 186, 62 },
    { 208, 62 },
    { 247, 62 },
    { 102, 74 },
    { 136, 74 },
    { 186, 74 },
    { 235, 74 },
    { 260, 70 },
    { 271, 70 },
    { 235, 86 },
    { 247, 86 },
    { 260, 91 },
    { 271, 91 },
    { 235, 104 },
    { 247, 104 },
    { 260, 109 },
    { 271, 109 },
    { 81, 120 },
    { 128, 120 },
    { 235, 120 },
    { 93, 131 },
    { 128, 131 },
    { 175, 131 },
    { 217, 131 },
    { 247, 131 },
    { 104, 144 },
    { 146, 144 },
    { 175, 144 },
    { 217, 144 },
    { 260, 144 },
    { 81, 163 },
    { 93, 163 },
    { 104, 160 },
    { 116, 160 },
    { 116, 155 },
    { 146, 155 },
    { 271, 155 },
    { 93, 176 },
    { 104, 176 },
    { 116, 170 },
    { 223, 170 },
    { 104, 182 },
    { 179, 182 },
    { 198, 189 },
    { 223, 189 },
    { 93, 192 },
    { 143, 192 },
    { 81, 201 },
    { 151, 201 },
    { 163, 213 },
    { 70, 27 },
    { 70, 39 },
    { 70, 55 },
    { 70, 63 },
    { 70, 79 },
    { 286, 189 },
    { 264, 189 },
    { 76, 38 },
    { 82, 51 },
    { 89, 51 },
    { 76, 62 },
    { 82, 74 },
    { 89, 74 },
    { -1, -1 },
};

/// First-wiring socket 1's current route, from its battery through the diagram to ground.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100FirstWiringSocket1Route[36] = {
    69,
    75,
    ACTOR_548100_ROUTE_GAP,
    3,
    4,
    9,
    10,
    11,
    17,
    18,
    19,
    12,
    13,
    14,
    25,
    29,
    30,
    34,
    33,
    47,
    46,
    45,
    40,
    39,
    36,
    35,
    48,
    49,
    55,
    63,
    64,
    66,
    67,
    ACTOR_548100_ROUTE_END,
    0,
    0,
};

/// First-wiring socket 2's current route, from its battery to the door output.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100FirstWiringSocket2Route[24] = {
    70,
    76,
    77,
    ACTOR_548100_ROUTE_GAP,
    8,
    9,
    4,
    5,
    6,
    7,
    26,
    30,
    29,
    33,
    34,
    54,
    53,
    52,
    51,
    57,
    58,
    ACTOR_548100_ROUTE_END,
    0,
    0,
};

/// First-wiring socket 3's current route through the diagram.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100FirstWiringSocket3Route[28] = {
    71,
    78,
    ACTOR_548100_ROUTE_GAP,
    15,
    16,
    17,
    11,
    12,
    19,
    20,
    28,
    27,
    31,
    32,
    42,
    41,
    40,
    45,
    44,
    43,
    50,
    56,
    59,
    60,
    61,
    62,
    ACTOR_548100_ROUTE_END,
    0,
};

/// First-wiring socket 4's current route, from its battery through the diagram to ground.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100FirstWiringSocket4Route[24] = {
    72,
    79,
    80,
    ACTOR_548100_ROUTE_GAP,
    21,
    22,
    23,
    24,
    27,
    28,
    32,
    31,
    37,
    36,
    39,
    38,
    49,
    48,
    65,
    66,
    67,
    ACTOR_548100_ROUTE_END,
    0,
    0,
};

/// Second-wiring socket 1's current route, from its battery to the door output.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100SecondWiringSocket1Route[28] = {
    69,
    75,
    ACTOR_548100_ROUTE_GAP,
    3,
    4,
    9,
    10,
    5,
    6,
    13,
    14,
    25,
    26,
    30,
    29,
    33,
    34,
    54,
    53,
    44,
    43,
    50,
    51,
    57,
    58,
    ACTOR_548100_ROUTE_END,
    0,
    0,
};

/// Second-wiring socket 2's current route, from its battery through the diagram to ground.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
static u8 _gActor548100SecondWiringSocket2Route[28] = {
    70,
    76,
    77,
    ACTOR_548100_ROUTE_GAP,
    8,
    9,
    4,
    5,
    10,
    11,
    17,
    18,
    23,
    24,
    27,
    28,
    32,
    31,
    37,
    36,
    39,
    38,
    49,
    48,
    65,
    66,
    67,
    ACTOR_548100_ROUTE_END,
};

/// Second-wiring socket 3's current route, from its battery through the diagram to ground.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
static u8 _gActor548100SecondWiringSocket3Route[36] = {
    71,
    78,
    ACTOR_548100_ROUTE_GAP,
    15,
    16,
    22,
    23,
    18,
    19,
    12,
    13,
    6,
    7,
    26,
    25,
    29,
    30,
    34,
    33,
    47,
    46,
    41,
    40,
    45,
    44,
    53,
    52,
    51,
    50,
    56,
    55,
    63,
    64,
    66,
    67,
    ACTOR_548100_ROUTE_END,
};

/// Second-wiring socket 4's current route, from its battery to the passage output.
///
/// Byte node ids end at zero; 0xFF escapes the next node across a gap.
/// Bytes after the first zero are retained storage, with no established consumer.
static u8 _gActor548100SecondWiringSocket4Route[36] = {
    72,
    79,
    80,
    ACTOR_548100_ROUTE_GAP,
    21,
    22,
    16,
    17,
    11,
    12,
    19,
    20,
    28,
    27,
    31,
    32,
    42,
    41,
    46,
    45,
    40,
    39,
    36,
    35,
    48,
    49,
    55,
    56,
    59,
    60,
    61,
    62,
    ACTOR_548100_ROUTE_END,
    0,
    0,
    0,
};

/// Independent main-feed route powering the passage in every first-wiring circuit.
///
/// Byte node ids end at zero. Bytes after the first zero are retained storage,
/// with no established consumer. The second wiring does not use this route.
static u8 _gActor548100MainFeedRoute[8] = {
    68,
    1,
    2,
    73,
    74,
    ACTOR_548100_ROUTE_END,
    0,
    0,
};

/// Node-sequence lookup by route id: 0 unused, 1..4 first wiring, 5..8 second, 9 main feed.
///
/// Battery routes correspond to sockets 1..4 within each wiring. Each sequence
/// ends at zero; 0xFF escapes the next node, moving across a gap without a wire.
static const u8* _gActor548100Routes[ACTOR_548100_ROUTE_COUNT] = {
    NULL,
    _gActor548100FirstWiringSocket1Route,
    _gActor548100FirstWiringSocket2Route,
    _gActor548100FirstWiringSocket3Route,
    _gActor548100FirstWiringSocket4Route,
    _gActor548100SecondWiringSocket1Route,
    _gActor548100SecondWiringSocket2Route,
    _gActor548100SecondWiringSocket3Route,
    _gActor548100SecondWiringSocket4Route,
    _gActor548100MainFeedRoute,
};

/// Borrowed circuit record selected for the current wiring and sockets; NULL before first lookup.
///
/// Selection advances within an overlay-owned table each frame. Consumers
/// read its legs and output flags; the pointer lasts only while this overlay is loaded.
/// The first wiring retains its terminator when no arrangement matches; the
/// second wiring requires at most two occupied sockets to stay within its table.
static const _Actor548100Circuit* _gActor548100CurrentCircuit = NULL;

/// Colour brightness numerator over 32, initialized to 16 on each panel visit.
///
/// Drawing uses one level for the frame, then advances the signed halfword.
/// Normal phases cycle 16..32; other phase values decay positive levels to zero.
static s16 _gActor548100ColorPulse = 0;

/// Colour-pulse phase (0 rising, 1 falling); other values select decay to zero.
///
/// Opening the panel resets rising. No producer of the decay values is established.
static u8 _gActor548100ColorPulsePhase = ACTOR_548100_PULSE_RISING;

/// Current frame's red FLOW channel, stored as its low eight RGB bits.
static u8 _gActor548100FlowRed = 0;

/// Current frame's green FLOW channel, stored as its low eight RGB bits.
static u8 _gActor548100FlowGreen = 0;

/// Current frame's blue FLOW channel, stored as its low eight RGB bits.
static u8 _gActor548100FlowBlue = 0;

/// Current frame's stopped-wire red RGB byte, pulsed from authored brightness 60.
///
/// Recomputed before drawing; normal pulse levels 16..32 give 30..60.
/// Stopped wires and the unpowered portion of a moving front read this byte.
static u8 _gActor548100StopRed = 0;

/// Current frame's stopped-wire green RGB byte, pulsed from authored brightness 60.
///
/// Normal pulse levels 16..32 give 30..60; decay brings it down to zero.
/// The colour scaler writes its low eight bits through a signed-byte view.
static u8 _gActor548100StopGreen = 0;

/// Current frame's stopped-wire blue RGB byte, pulsed from authored brightness 16.
///
/// Normal pulse levels 16..32 give 8..16; decay brings it down to zero.
/// The colour scaler writes its low eight bits through a signed-byte view.
static u8 _gActor548100StopBlue = 0;

/// Current frame's red RGB byte for short-circuit wires.
///
/// Recomputed before wire drawing as the authored red channel times the pulse
/// numerator over 32, retaining the low eight bits. Normal pulse levels 16..32
/// produce 80..160 from the authored value 160; the GPU consumes it unsigned.
static u8 _gActor548100ShortRed = 0;

/// Current frame's green RGB byte for short-circuit wires.
///
/// Recomputed before wire drawing as the authored green channel times the pulse
/// numerator over 32, retaining the low eight bits. Normal pulse levels 16..32
/// produce 8..16 from the authored value 16; decay brings it down to zero.
static u8 _gActor548100ShortGreen = 0;

/// Current frame's blue RGB byte for short-circuit wires.
///
/// Recomputed before wire drawing as the authored blue channel times the pulse
/// numerator over 32, retaining the low eight bits. Normal pulse levels 16..32
/// produce 16..32 from the authored value 32; decay brings it down to zero.
static u8 _gActor548100ShortBlue = 0;

/// Undirected node-pair lookup of byte indices into the panel wire table.
///
/// Key = previousNode * 100 + currentNode. Opening the panel writes both
/// directions of each authored wire; only those pairs are valid. Zero is a
/// valid wire index, not an absent-edge sentinel. Authored keys reach 8079;
/// the full extent beyond that is unproven and the existing storage is retained.
static u8 _gActor548100WireIndexByNodePair[10000] = { 0 };

static void _actor548100DrawEdge(const _Actor548100Edge* edge);

#include "../../shared/room/action_prompt_move_cursors.inc.c"

/// Label for the short-circuit wire color's RGB row, including the terminating NUL.
static const char _gActor548100ShortWireLabel[] = "Short";
/// Label for the stopped-wire color's RGB row, including the terminating NUL.
static const char _gActor548100StopWireLabel[] = "Stop";
/// Label for the flowing-wire color's RGB row, including the terminating NUL.
static const char _gActor548100FlowWireLabel[] = "Flow";

/// Mine power-panel callbacks indexed by `ACTOR_548100_STATE_*`.
///
/// The eleven states initialize panel work, arm the cursor, scan hotspots, open
/// commands, handle a choice, exit, place a battery, wait for battery return, wait
/// for the switch caption, animate current flow and wait for its completion caption.
static const TaskFuncTable11 _gActor548100PanelStateHandlers = {
    .funcs = {
        [ACTOR_548100_STATE_INIT_PANEL]              = _actor548100InitPanel,
        [ACTOR_548100_STATE_ARM_CURSOR]              = _actor548100ArmCursor,
        [ACTOR_548100_STATE_SCAN_HOTSPOTS]           = _actor548100ScanHotspots,
        [ACTOR_548100_STATE_OPEN_COMMANDS]           = _actor548100OpenHotspotCommands,
        [ACTOR_548100_STATE_HANDLE_CHOICE]           = _actor548100HandleHotspotChoice,
        [ACTOR_548100_STATE_EXIT]                    = _actor548100ExitPanel,
        [ACTOR_548100_STATE_PLACE_BATTERY]           = _actor548100PlaceQueuedBattery,
        [ACTOR_548100_STATE_WAIT_BATTERY_RETURN]     = _actor548100WaitBatteryReturn,
        [ACTOR_548100_STATE_WAIT_SWITCH_CAPTION]     = _actor548100WaitSwitchCaption,
        [ACTOR_548100_STATE_ANIMATE_FLOW]            = _actor548100AnimateCircuitFlow,
        [ACTOR_548100_STATE_WAIT_COMPLETION_CAPTION] = _actor548100WaitCompletionCaption,
    },
};

#include "../../shared/room/action_prompt_draw_cursor.inc.c"

/// Holds scripted player control and hides player/HUD for the open panel.
///
/// Requires a live session and player model. Hide mode requests the parent's
/// primitive buffer and propagates its draw flags to attachments. The panel
/// exit restores player/HUD/control and releases the separate menu-display hold.
static inline void _actor548100HoldPanelPresentation(void)
{
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
}

/// Opens the mine power-panel interface and takes control of its menu presentation.
///
/// Allocates owned zeroed work or kills the panel task on failure. Spawns the
/// action prompt, installs messages and advances to cursor arming. First use
/// seeds the panel/socket flags; each visit clears hotspot hits and rebuilds the
/// wire graph. Holds menu display and scripted player control, hides the player
/// and HUD, and sets saved view 4. The panel exit releases those holds. Prompt
/// spawn failure is retained without a local check.
static void _actor548100InitPanel(Task* task)
{
    enum {
        ACTOR_548100_PANEL_SAVED_VIEW    = 4,
        ACTOR_548100_INITIAL_COLOR_PULSE = 16,
    };
    _Actor548100Work*    work;
    ActionPromptHotspot* hotspot;
    ActionPromptHotspot* hotspots;

    work = memCalloc(sizeof(_Actor548100Work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                = taskSpawnFromTable(&_gActor548100PanelPromptTaskDesc, 0, 1, 0);
    task->msgTable                                         = _gActor548100PanelMessages;
    task->work                                             = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACTOR_548100_PANEL_SAVED_VIEW;
    task->state                                           += ACTOR_548100_STATE_ARM_CURSOR - ACTOR_548100_STATE_INIT_PANEL;
    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 0) {
        gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE, 1);
        gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SOCKET_4, 1);
    }
    work->usedItem = 0;
    // Begin the panel menu hold and reset room-owned hotspot feedback.
    displayAcquireMenuHold();
    hotspots = _gActor548100PanelHotspots;
    for (hotspot = hotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    _gActor548100ColorPulse      = ACTOR_548100_INITIAL_COLOR_PULSE;
    _gActor548100ColorPulsePhase = ACTOR_548100_PULSE_RISING;
    _actor548100InitWireGraph();
    _actor548100HoldPanelPresentation();
}

/// Lets the player select a panel hotspot or cancel while no caption is playing.
///
/// State 2 of the panel task. Uses the first action cursor; confirm latches
/// the first hit entry's choice and prompt kind before opening its commands.
/// Cancel enters panel exit. Captions hide and stop the cursor without scanning.
static void _actor548100ScanHotspots(Task* task)
{
    ActionPrompt*        prompt  = gActionPromptSlots;
    ActionPromptHotspot* hotspot = _gActor548100PanelHotspots;
    _Actor548100Work*    work    = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    work->choice        = 0;
    if (_actionPromptHitTestFirstChoice(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = ACTOR_548100_STATE_OPEN_COMMANDS;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = ACTOR_548100_STATE_EXIT;
    }
}

/// Offers the selected power-panel socket's battery as a room pickup.
///
/// Requires live panel work selecting a filled socket 1..4, panel power off
/// and room CAP resources loaded through playback. Socket content 1 selects pickup
/// object 4 and caption variant 2; every other content selects object 5 and
/// variant 5. Marks that object available in the live save's stage and requests
/// command 6 in place, ignoring caption admission failure. Records the object
/// in `work->pickupObject` for the collection wait; the socket contents and
/// inventory are unchanged here.
static inline void _actor548100OfferSocketBattery(_Actor548100Work* work)
{
    enum {
        ACTOR_548100_FIRST_BATTERY_PICKUP     = 4,
        ACTOR_548100_SECOND_BATTERY_PICKUP    = 5,
        ACTOR_548100_BATTERY_PICKUP_AVAILABLE = 1,
    };
    s32 captionVariant;

    if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == ACTOR_548100_SOCKET_FIRST_BATTERY) {
        work->pickupObject = ACTOR_548100_FIRST_BATTERY_PICKUP;
        captionVariant     = ACTOR_548100_CAP_SOCKET_FIRST_PICKUP;
    } else {
        work->pickupObject = ACTOR_548100_SECOND_BATTERY_PICKUP;
        captionVariant     = ACTOR_548100_CAP_SOCKET_SECOND_PICKUP;
    }
    areaSetCurrentObjectState(work->pickupObject, ACTOR_548100_BATTERY_PICKUP_AVAILABLE);
    capStartSequenceSlot(ACTOR_548100_CAP_COMMAND_SOCKET, CAP_PLAYBACK_IN_PLACE, captionVariant);
}

/// Handles a confirmed panel hotspot action or a battery queued by the item menu.
///
/// State 4 requires initialized work and the first action cursor. Hides/stops
/// that cursor. Sockets 1..4 offer a removable battery only while power is off;
/// empty or powered sockets play explanatory captions. Choice 5 starts the
/// switch caption; 6..9 describe the power light, power lines, ground wire and
/// gate locks. A queued item is placed on the next state tick after menu closure.
/// All other paths return to hotspot scanning, whose caption gate keeps input off.
static void _actor548100HandleHotspotChoice(Task* task)
{
    enum {
        ACTOR_548100_CAP_COMMAND_POWER_LIGHT = 4,
        ACTOR_548100_CAP_COMMAND_SWITCH      = 5,
        ACTOR_548100_CAP_COMMAND_POWER_LINES = 7,
        ACTOR_548100_CAP_COMMAND_GATE_LOCKS  = 8,
        ACTOR_548100_CAP_COMMAND_GROUND_WIRE = 9,
    };
    _Actor548100Work* work = task->work;

    gActionPromptSlots[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    gActionPromptSlots[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        switch (work->choice) {
            case ACTOR_548100_CHOICE_SOCKET_1:
            case ACTOR_548100_CHOICE_SOCKET_2:
            case ACTOR_548100_CHOICE_SOCKET_3:
            case ACTOR_548100_CHOICE_SOCKET_4:
                if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == ACTOR_548100_SOCKET_EMPTY) {
                    capStartSequenceSlot(ACTOR_548100_CAP_COMMAND_SOCKET, CAP_PLAYBACK_IN_PLACE, ACTOR_548100_CAP_SOCKET_EMPTY);
                } else if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
                    capStartSequenceSlot(ACTOR_548100_CAP_COMMAND_SOCKET, CAP_PLAYBACK_DISPLAY_TRANSITION, ACTOR_548100_CAP_SOCKET_LOCKED);
                } else {
                    _actor548100OfferSocketBattery(work);
                    task->state = ACTOR_548100_STATE_WAIT_BATTERY_RETURN;
                    return;
                }
                break;
            case ACTOR_548100_CHOICE_SWITCH:
                capRunCommand(ACTOR_548100_CAP_COMMAND_SWITCH, CAP_PLAYBACK_IN_PLACE);
                task->state = ACTOR_548100_STATE_WAIT_SWITCH_CAPTION;
                return;
            case ACTOR_548100_CHOICE_POWER_LIGHT:
                capRunCommand(ACTOR_548100_CAP_COMMAND_POWER_LIGHT, CAP_PLAYBACK_IN_PLACE);
                break;
            case ACTOR_548100_CHOICE_POWER_LINES:
                capRunCommand(ACTOR_548100_CAP_COMMAND_POWER_LINES, CAP_PLAYBACK_IN_PLACE);
                break;
            case ACTOR_548100_CHOICE_GROUND_WIRE:
                capRunCommand(ACTOR_548100_CAP_COMMAND_GROUND_WIRE, CAP_PLAYBACK_IN_PLACE);
                break;
            case ACTOR_548100_CHOICE_GATE_LOCKS:
                capRunCommand(ACTOR_548100_CAP_COMMAND_GATE_LOCKS, CAP_PLAYBACK_IN_PLACE);
                break;
        }
        task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
    } else if (work->usedItem != 0) {
        task->state = ACTOR_548100_STATE_PLACE_BATTERY;
    } else {
        task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
    }
}

/// Waits for the switch caption, then turns the panel on or off from its completion key.
///
/// State 8 follows the switch command. Turning on selects and measures the
/// circuit, orders its battery legs by length, clears all three distances and
/// enters the flow animation. Turning off clears the switch flag; other keys
/// simply return to hotspot scanning. The live task must have panel work.
static void _actor548100WaitSwitchCaption(Task* task)
{
    _Actor548100Work* work = task->work;
    s32               firstLegLength;
    s32               secondLegLength;

    if (capIsBusy() == 0) {
        if (capGetVariantKey() == ACTOR_548100_CAPTION_SWITCH_ON) {
            if (gameFlagGetNibble(GAME_FLAG_110) != 0) {
                itemSetIdentified(ACTOR_548100_BATTERY_FIRST_ITEM, 1);
                itemSetIdentified(ACTOR_548100_BATTERY_SECOND_ITEM, 1);
            }
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 1);
            task->state = ACTOR_548100_STATE_ANIMATE_FLOW;
            _actor548100SelectCircuit();
            work->thirdLength = _actor548100MeasureLeg(_gActor548100CurrentCircuit->leg[2].route, _gActor548100CurrentCircuit->leg[2].stopNode);
            firstLegLength    = _actor548100MeasureLeg(_gActor548100CurrentCircuit->leg[0].route, _gActor548100CurrentCircuit->leg[0].stopNode);
            secondLegLength   = _actor548100MeasureLeg(_gActor548100CurrentCircuit->leg[1].route, _gActor548100CurrentCircuit->leg[1].stopNode);
            // Keep the two battery legs in length order so their fronts arrive together.
            if (secondLegLength < firstLegLength) {
                work->longLength        = firstLegLength;
                work->shortLength       = secondLegLength;
                work->longLeg.route     = _gActor548100CurrentCircuit->leg[0].route;
                work->shortLeg.route    = _gActor548100CurrentCircuit->leg[1].route;
                work->longLeg.stopNode  = _gActor548100CurrentCircuit->leg[0].stopNode;
                work->shortLeg.stopNode = _gActor548100CurrentCircuit->leg[1].stopNode;
            } else {
                work->longLength        = secondLegLength;
                work->shortLength       = firstLegLength;
                work->longLeg.route     = _gActor548100CurrentCircuit->leg[1].route;
                work->shortLeg.route    = _gActor548100CurrentCircuit->leg[0].route;
                work->longLeg.stopNode  = _gActor548100CurrentCircuit->leg[1].stopNode;
                work->shortLeg.stopNode = _gActor548100CurrentCircuit->leg[0].stopNode;
            }
            work->longProgress  = 0;
            work->shortProgress = 0;
            work->thirdProgress = 0;
            return;
        }
        if (capGetVariantKey() == ACTOR_548100_CAPTION_SWITCH_OFF) {
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 0);
        }
        task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
    }
}

/// Draws the mine power panel and publishes its settled circuit outputs.
///
/// Requires initialized panel work, graph geometry and the selected circuit.
/// During state 9 the work's three pixel distances animate the wires and preview
/// lit outputs; persistent output flags remain clear until that state finishes.
/// Draws occupied sockets and the thrown switch, then advances the shared colour
/// pulse for the next frame. Appends packets to the current GPU primitive buffer.
static void _actor548100DrawPanel(Task* task)
{
    enum {
        // Dithered additive blending; 4-bit page at VRAM (640, 0), display-area drawing off.
        ACTOR_548100_WIRE_DRAW_MODE                = _get_mode(0, 1, getTPage(0, GPU_BLEND_ADD, 640, 0)),
        ACTOR_548100_OT_WIRE_DRAW_MODE             = 0x3FD,
        ACTOR_548100_COLOR_PULSE_MIN               = 16,
        ACTOR_548100_PASSAGE_UNPOWERED             = 0,
        ACTOR_548100_PASSAGE_POWERED_FIRST_WIRING  = 2,
        ACTOR_548100_PASSAGE_POWERED_SECOND_WIRING = 3,
    };

    _Actor548100Work*          work;
    const _Actor548100TexRect* socketPiece;
    DR_MODE*                   wireDrawMode;
    const _Actor548100Circuit* circuit;
    s32                        socketIndex;
    s32                        doorLit;
    s32                        passageLit;

    socketIndex = 0;
    socketPiece = _gActor548100PanelPieces;
    work        = task->work;
    for (; socketIndex < ACTOR_548100_SOCKET_COUNT; socketIndex++, socketPiece++) {
        if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(socketIndex + 1)) != 0) {
            _actor548100DrawPanelPiece(socketPiece);
        }
    }

    wireDrawMode   = gGpuPrimCursor;
    gGpuPrimCursor = wireDrawMode + 1;
    setlen(wireDrawMode, 1);
    wireDrawMode->code[0] = ACTOR_548100_WIRE_DRAW_MODE;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRE_DRAW_MODE], wireDrawMode);

    // The signed-byte scaler writes the same low eight bits into the unsigned RGB storage.
    _actor548100ScaleFlowColor(_gActor548100ColorPulse, (s8*)&_gActor548100FlowRed, (s8*)&_gActor548100FlowGreen, (s8*)&_gActor548100FlowBlue);
    _actor548100ScaleStopColor(_gActor548100ColorPulse, (s8*)&_gActor548100StopRed, (s8*)&_gActor548100StopGreen, (s8*)&_gActor548100StopBlue);
    _actor548100ScaleShortColor(_gActor548100ColorPulse, (s8*)&_gActor548100ShortRed, (s8*)&_gActor548100ShortGreen, (s8*)&_gActor548100ShortBlue);
    // Rebuild this frame from stopped wires before applying the selected circuit.
    _actor548100ResetWireStates();
    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, ACTOR_548100_PASSAGE_UNPOWERED);
    gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
        if (task->state != ACTOR_548100_STATE_ANIMATE_FLOW) {
            circuit = _gActor548100CurrentCircuit;
            if (circuit->leg[0].route != ACTOR_548100_ROUTE_UNUSED) {
                _actor548100SetLegComplete(circuit->leg[0].route, circuit->leg[0].stopNode);
                circuit = _gActor548100CurrentCircuit;
            }
            if (circuit->leg[1].route != ACTOR_548100_ROUTE_UNUSED) {
                _actor548100SetLegComplete(circuit->leg[1].route, circuit->leg[1].stopNode);
            }
            circuit = _gActor548100CurrentCircuit;
            if (circuit->leg[2].route != ACTOR_548100_ROUTE_UNUSED) {
                _actor548100SetLegComplete(circuit->leg[2].route, circuit->leg[2].stopNode);
            }
            if (_gActor548100CurrentCircuit->powersDoor != 0) {
                _actor548100DrawOutputIndicator(ACTOR_548100_OUTPUT_DOOR);
                gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 1);
            }
            if (_gActor548100CurrentCircuit->powersPassage != 0) {
                _actor548100DrawOutputIndicator(ACTOR_548100_OUTPUT_PASSAGE);
                if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == ACTOR_548100_WIRING_SECOND) {
                    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, ACTOR_548100_PASSAGE_POWERED_SECOND_WIRING);
                } else {
                    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, ACTOR_548100_PASSAGE_POWERED_FIRST_WIRING);
                }
            }
        } else {
            // Preview completed outputs while the switch-on current is still advancing.
            circuit    = _gActor548100CurrentCircuit;
            doorLit    = 0;
            passageLit = 0;
            if (circuit->leg[2].route != ACTOR_548100_ROUTE_UNUSED) {
                _actor548100SetLegFlowProgress(circuit->leg[2].route, circuit->leg[2].stopNode, work->thirdProgress);
                passageLit = work->thirdProgress == work->thirdLength;
            }
            if (work->longLeg.route != ACTOR_548100_ROUTE_UNUSED) {
                if (work->longProgress != work->longLength) {
                    _actor548100SetLegFlowProgress(work->longLeg.route, work->longLeg.stopNode, work->longProgress);
                } else {
                    _actor548100SetLegComplete(work->longLeg.route, work->longLeg.stopNode);
                }
            }
            if (work->shortLeg.route != ACTOR_548100_ROUTE_UNUSED) {
                if (work->shortProgress != work->shortLength) {
                    _actor548100SetLegFlowProgress(work->shortLeg.route, work->shortLeg.stopNode, work->shortProgress);
                } else {
                    _actor548100SetLegComplete(work->shortLeg.route, work->shortLeg.stopNode);
                }
            }
            if (work->longProgress == work->longLength && work->longLeg.stopNode == ACTOR_548100_STOP_NODE_NONE && work->longLeg.route != ACTOR_548100_ROUTE_UNUSED) {
                doorLit    = _gActor548100CurrentCircuit->powersDoor;
                passageLit = passageLit || _gActor548100CurrentCircuit->powersPassage;
            }
            if (doorLit != 0) {
                _actor548100DrawOutputIndicator(ACTOR_548100_OUTPUT_DOOR);
            }
            if (passageLit != 0) {
                _actor548100DrawOutputIndicator(ACTOR_548100_OUTPUT_PASSAGE);
            }
        }
    }

    _actor548100DrawWires();
    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == 0) {
        gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, ACTOR_548100_PASSAGE_UNPOWERED);
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);
    } else {
        _actor548100DrawPanelPiece(&_gActor548100PanelPieces[ACTOR_548100_PIECE_SWITCH]);
    }

    // Advance brightness after drawing so every wire uses the same pulse level.
    if (_gActor548100ColorPulsePhase == ACTOR_548100_PULSE_RISING) {
        if (++_gActor548100ColorPulse >= ACTOR_548100_COLOR_PULSE_ONE) {
            _gActor548100ColorPulsePhase = ACTOR_548100_PULSE_FALLING;
        }
    } else if (_gActor548100ColorPulsePhase == ACTOR_548100_PULSE_FALLING) {
        if (--_gActor548100ColorPulse <= ACTOR_548100_COLOR_PULSE_MIN) {
            _gActor548100ColorPulsePhase = ACTOR_548100_PULSE_RISING;
        }
    } else if (_gActor548100ColorPulse > 0) {
        _gActor548100ColorPulse--;
    }
}

/// Empty per-frame panel hook between circuit selection and drawing.
///
/// The task argument is unused.
static void _actor548100PostStateHook(Task* unusedTask)
{
}

#include "../../shared/room/action_prompt_outline_rect.inc.c"

/// Selects the current wiring's circuit with exactly the filled battery sockets.
///
/// The result borrows a record from the authored circuit table. Normal input
/// must have at most two filled sockets: the second wiring has no terminator
/// beyond its eleven empty, single-socket and socket-pair arrangements.
static void _actor548100SelectCircuit(void)
{
    s32 circuitSocketMask;
    s32 filledSocketMask;
    s32 socketIndex;

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == ACTOR_548100_WIRING_FIRST) {
        _gActor548100CurrentCircuit = _gActor548100FirstWiringCircuits;
    } else {
        _gActor548100CurrentCircuit = _gActor548100SecondWiringCircuits;
    }
    while (_gActor548100CurrentCircuit->socketA != ACTOR_548100_CIRCUIT_END) {
        circuitSocketMask = 0;
        if (_gActor548100CurrentCircuit->socketA != 0) {
            circuitSocketMask = 1 << (_gActor548100CurrentCircuit->socketA - 1);
        }
        if (_gActor548100CurrentCircuit->socketB != 0) {
            circuitSocketMask |= 1 << (_gActor548100CurrentCircuit->socketB - 1);
        }
        filledSocketMask = 0;
        for (socketIndex = 0; socketIndex < ACTOR_548100_SOCKET_COUNT; socketIndex++) {
            if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(socketIndex + 1)) != 0) {
                filledSocketMask |= 1 << socketIndex;
            }
        }
        if (circuitSocketMask == filledSocketMask) {
            break;
        }
        _gActor548100CurrentCircuit++;
    }
}

/// Initializes a translucent glow quad fading from black at vertices 0/1 to RGB at 2/3.
///
/// Borrows one writable packet. Sets its GPU length, primitive code and vertex
/// colours; coordinates, allocation and ordering-table linkage belong to the
/// caller. RGB components are unsigned bytes and the blend mode is supplied
/// separately by the panel's draw-mode packet.
static inline void _actor548100InitGlowQuad(POLY_G4* glow, u8 red, u8 green, u8 blue)
{
    setPolyG4(glow);
    setSemiTrans(glow, 1);
    glow->r0 = 0;
    glow->g0 = 0;
    glow->b0 = 0;
    glow->r1 = 0;
    glow->g1 = 0;
    glow->b1 = 0;
    glow->r2 = red;
    glow->g2 = green;
    glow->b2 = blue;
    glow->r3 = red;
    glow->g3 = green;
    glow->b3 = blue;
}

/// Draws a panel wire with a two-pixel colour core and a three-pixel glow fringe.
///
/// Node ids index the authored diagram points; RGB channels are unsigned bytes.
/// Geometry uses the dominant screen axis and the diagram's (158, 118) origin.
/// Appends one flat and four Gouraud quads to the current primitive buffer;
/// the caller supplies the draw area and semitransparency mode.
static void _actor548100DrawWire(s32 nodeA, s32 nodeB, u8 red, u8 green, u8 blue)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s32      startX;
    s32      startY;
    s32      endX;
    s32      endY;
    s32      horizontalSpan;
    s32      swapCoord;
    s32      x0;
    s32      y0;
    s32      x1;
    s32      y1;
    s32      x2;
    s32      y2;
    s32      x3;
    s32      y3;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0       = red;
    quad->g0       = green;
    quad->b0       = blue;
    startX         = _gActor548100DiagramNodes[nodeA].vx - ACTOR_548100_WIRE_ORIGIN_X;
    startY         = _gActor548100DiagramNodes[nodeA].vy - ACTOR_548100_WIRE_ORIGIN_Y;
    endX           = _gActor548100DiagramNodes[nodeB].vx - ACTOR_548100_WIRE_ORIGIN_X;
    endY           = _gActor548100DiagramNodes[nodeB].vy - ACTOR_548100_WIRE_ORIGIN_Y;
    horizontalSpan = startX - endX;
    if (horizontalSpan < 0) {
        horizontalSpan = endX - startX;
    }
    if (ABS(startY - endY) < horizontalSpan) {
        if (endX < startX) {
            swapCoord = startX;
            startX    = endX;
            endX      = swapCoord;
            swapCoord = startY;
            startY    = endY;
            endY      = swapCoord;
        }
        x0 = startX + 1;
        y0 = startY - 1;
        x1 = endX - 1;
        y1 = endY - 1;
        x2 = x0;
        y2 = startY + 1;
        x3 = x1;
        y3 = endY + 1;
    } else {
        if (endY < startY) {
            swapCoord = startX;
            startX    = endX;
            endX      = swapCoord;
            swapCoord = startY;
            startY    = endY;
            endY      = swapCoord;
        }
        x0 = startX - 1;
        y0 = startY + 1;
        x1 = startX + 1;
        y1 = y0;
        x2 = endX - 1;
        y2 = endY - 1;
        x3 = endX + 1;
        y3 = y2;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y1;
    quad->x2 = x2;
    quad->y2 = y2;
    quad->x3 = x3;
    quad->y3 = y3;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], quad);

    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    _actor548100InitGlowQuad(top, red, green, blue);
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y1 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y1;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    _actor548100InitGlowQuad(left, red, green, blue);
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x2 - 3;
    left->y1 = y2 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x2;
    left->y3 = y2;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    _actor548100InitGlowQuad(right, red, green, blue);
    right->x0 = x1 + 3;
    right->y0 = y1 - 3;
    right->x1 = x3 + 3;
    right->y1 = y3 + 3;
    right->x2 = x1;
    right->y2 = y1;
    right->x3 = x3;
    right->y3 = y3;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    _actor548100InitGlowQuad(bottom, red, green, blue);
    bottom->x0 = x2 - 3;
    bottom->y0 = y2 + 3;
    bottom->x1 = x3 + 3;
    bottom->y1 = y3 + 3;
    bottom->x2 = x2;
    bottom->y2 = y2;
    bottom->x3 = x3;
    bottom->y3 = y3;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], bottom);
}

/// Draws a wire in its current flow, stopped or short-circuit colour.
///
/// Partial flow clips two copies at `flowLength` dominant-axis pixels from
/// the entry node. Drawing executes in reverse packet-link order and restores
/// the full draw area afterwards. Absent and unrecognized states draw nothing;
/// the borrowed edge and authored node points are not changed.
static void _actor548100DrawEdge(const _Actor548100Edge* edge)
{
    RECT     rect;
    DR_AREA* area;
    s32      fromX;
    s32      fromY;
    s32      toX;
    s32      toY;
    s32      splitCoord;
    s32      direction;
    s32      fromNode;
    s32      toNode;

    fromNode = edge->nodeA;
    toNode   = edge->nodeB;
    switch (edge->state) {
        case ACTOR_548100_EDGE_STATE_SHORT:
            _actor548100DrawWire(fromNode, toNode, _gActor548100ShortRed, _gActor548100ShortGreen, _gActor548100ShortBlue);
            break;
        case ACTOR_548100_EDGE_STATE_FLOW:
            _actor548100DrawWire(fromNode, toNode, _gActor548100FlowRed, _gActor548100FlowGreen, _gActor548100FlowBlue);
            break;
        case ACTOR_548100_EDGE_STATE_STOP:
            _actor548100DrawWire(fromNode, toNode, _gActor548100StopRed, _gActor548100StopGreen, _gActor548100StopBlue);
            break;
        case ACTOR_548100_EDGE_STATE_FLOW_FROM_B:
            fromNode = edge->nodeB;
            toNode   = edge->nodeA;
        case ACTOR_548100_EDGE_STATE_FLOW_FROM_A:
            // Prepending packets reverses execution: clip STOP, then FLOW, then restore.
            area           = gGpuPrimCursor;
            gGpuPrimCursor = area + 1;
            fromX          = _gActor548100DiagramNodes[fromNode].vx - ACTOR_548100_WIRE_ORIGIN_X;
            fromY          = _gActor548100DiagramNodes[fromNode].vy - ACTOR_548100_WIRE_ORIGIN_Y;
            toX            = _gActor548100DiagramNodes[toNode].vx - ACTOR_548100_WIRE_ORIGIN_X;
            toY            = _gActor548100DiagramNodes[toNode].vy - ACTOR_548100_WIRE_ORIGIN_Y;
            setRECT(&rect, 0, 0, DISPLAY_SCREEN_WIDTH, DISPLAY_SCREEN_HEIGHT);
            rect.y += gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], area);
            if (edge->vertical == 0) {
                direction = 1;
                if (toX < fromX) {
                    direction = -1;
                }
                splitCoord = fromX + direction * edge->flowLength;
                _actor548100DrawWire(fromNode, toNode, _gActor548100FlowRed, _gActor548100FlowGreen, _gActor548100FlowBlue);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (fromX < toX) {
                    setRECT(&rect, 0, 0, splitCoord + ACTOR_548100_SCREEN_CENTER_X, DISPLAY_SCREEN_HEIGHT);
                } else {
                    setRECT(&rect, splitCoord + ACTOR_548100_SCREEN_CENTER_X, 0, ACTOR_548100_SCREEN_CENTER_X - splitCoord, DISPLAY_SCREEN_HEIGHT);
                }
                rect.y += gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], area);
                _actor548100DrawWire(fromNode, toNode, _gActor548100StopRed, _gActor548100StopGreen, _gActor548100StopBlue);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (fromX < toX) {
                    setRECT(&rect, splitCoord + ACTOR_548100_SCREEN_CENTER_X, 0, ACTOR_548100_SCREEN_CENTER_X - splitCoord, DISPLAY_SCREEN_HEIGHT);
                } else {
                    setRECT(&rect, 0, 0, splitCoord + ACTOR_548100_SCREEN_CENTER_X, DISPLAY_SCREEN_HEIGHT);
                }
            } else {
                direction = 1;
                if (toY < fromY) {
                    direction = -1;
                }
                splitCoord = fromY + direction * edge->flowLength;
                _actor548100DrawWire(fromNode, toNode, _gActor548100FlowRed, _gActor548100FlowGreen, _gActor548100FlowBlue);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (fromY < toY) {
                    setRECT(&rect, 0, 0, DISPLAY_SCREEN_WIDTH, splitCoord + ACTOR_548100_SCREEN_CENTER_Y);
                } else {
                    setRECT(&rect, 0, splitCoord + ACTOR_548100_SCREEN_CENTER_Y, DISPLAY_SCREEN_WIDTH, ACTOR_548100_SCREEN_CENTER_Y - splitCoord);
                }
                rect.y += gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], area);
                _actor548100DrawWire(fromNode, toNode, _gActor548100StopRed, _gActor548100StopGreen, _gActor548100StopBlue);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (fromY < toY) {
                    setRECT(&rect, 0, splitCoord + ACTOR_548100_SCREEN_CENTER_Y, DISPLAY_SCREEN_WIDTH, ACTOR_548100_SCREEN_CENTER_Y - splitCoord);
                } else {
                    setRECT(&rect, 0, 0, DISPLAY_SCREEN_WIDTH, splitCoord + ACTOR_548100_SCREEN_CENTER_Y);
                }
            }
            rect.y += gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], area);
            break;
    }
}

/// Lights the panel's door or secret-passage output indicator in the flow colour.
///
/// `output` is `ACTOR_548100_OUTPUT_DOOR` or `ACTOR_548100_OUTPUT_PASSAGE`.
/// Appends a translucent rectangular core and four fading border quads to
/// the current primitive buffer.
static void _actor548100DrawOutputIndicator(s32 output)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s16      leftX;
    s16      rightX;
    s16      topY;
    s16      bottomY;
    u8       topRed;
    u8       topGreen;
    u8       topBlue;
    u8       leftRed;
    u8       leftGreen;
    u8       leftBlue;
    u8       rightRed;
    u8       rightGreen;
    u8       rightBlue;
    u8       bottomRed;
    u8       bottomGreen;
    u8       bottomBlue;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = _gActor548100FlowRed;
    quad->g0 = _gActor548100FlowGreen;
    quad->b0 = _gActor548100FlowBlue;
    leftX    = 0x43;
    if (output == ACTOR_548100_OUTPUT_DOOR) {
        topY    = 0x2E;
        rightX  = 0x68;
        bottomY = 0x39;
    } else {
        topY    = 0x42;
        rightX  = 0x68;
        bottomY = 0x4D;
    }
    quad->x0 = leftX;
    quad->y0 = topY;
    quad->x1 = rightX;
    quad->y1 = topY;
    quad->x2 = leftX;
    quad->y2 = bottomY;
    quad->x3 = rightX;
    quad->y3 = bottomY;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], quad);

    topRed         = _gActor548100FlowRed;
    topGreen       = _gActor548100FlowGreen;
    topBlue        = _gActor548100FlowBlue;
    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    _actor548100InitGlowQuad(top, topRed, topGreen, topBlue);
    top->x0 = leftX - 3;
    top->y0 = topY - 3;
    top->x1 = rightX + 3;
    top->y1 = topY - 3;
    top->x2 = leftX;
    top->y2 = topY;
    top->x3 = rightX;
    top->y3 = topY;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], top);

    leftRed        = _gActor548100FlowRed;
    leftGreen      = _gActor548100FlowGreen;
    leftBlue       = _gActor548100FlowBlue;
    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    _actor548100InitGlowQuad(left, leftRed, leftGreen, leftBlue);
    left->x0 = leftX - 3;
    left->y0 = topY - 3;
    left->x1 = leftX - 3;
    left->y1 = bottomY + 3;
    left->x2 = leftX;
    left->y2 = topY;
    left->x3 = leftX;
    left->y3 = bottomY;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], left);

    rightRed       = _gActor548100FlowRed;
    rightGreen     = _gActor548100FlowGreen;
    rightBlue      = _gActor548100FlowBlue;
    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    _actor548100InitGlowQuad(right, rightRed, rightGreen, rightBlue);
    right->x0 = rightX + 3;
    right->y0 = topY - 3;
    right->x1 = rightX + 3;
    right->y1 = bottomY + 3;
    right->x2 = rightX;
    right->y2 = topY;
    right->x3 = rightX;
    right->y3 = bottomY;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], right);

    bottomRed      = _gActor548100FlowRed;
    bottomGreen    = _gActor548100FlowGreen;
    bottomBlue     = _gActor548100FlowBlue;
    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    _actor548100InitGlowQuad(bottom, bottomRed, bottomGreen, bottomBlue);
    bottom->x0 = leftX - 3;
    bottom->y0 = bottomY + 3;
    bottom->x1 = rightX + 3;
    bottom->y1 = bottomY + 3;
    bottom->x2 = leftX;
    bottom->y2 = bottomY;
    bottom->x3 = rightX;
    bottom->y3 = bottomY;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], bottom);
}

/// Draws an eight-pixel flow-colour fringe around the panel's switch region.
///
/// The region is (-134, -98)..(-91, 101) in screen-centred pixels. Reserves a
/// half-bright flat core without linking it, then links four translucent glow
/// quads. No caller uses this retained drawer; the caller would supply the draw
/// area and blend mode.
static void _actor548100DrawSwitchRegionGlow(void)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    u8       red;
    u8       green;
    u8       blue;

    red   = _gActor548100FlowRed;
    green = _gActor548100FlowGreen;
    blue  = _gActor548100FlowBlue;

    // Preserve the reserved core packet: only the four fringe packets are linked.
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = red >> 1;
    quad->g0 = green >> 1;
    quad->b0 = blue >> 1;
    quad->x0 = -0x86;
    quad->y0 = -0x62;
    quad->x1 = -0x5B;
    quad->y1 = -0x62;
    quad->x2 = -0x86;
    quad->y2 = 0x65;
    quad->x3 = -0x5B;
    quad->y3 = 0x65;

    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    _actor548100InitGlowQuad(top, red, green, blue);
    top->x0 = -0x8e;
    top->y0 = -0x6a;
    top->x1 = -0x53;
    top->y1 = -0x6a;
    top->x2 = -0x86;
    top->y2 = -0x62;
    top->x3 = -0x5b;
    top->y3 = -0x62;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    _actor548100InitGlowQuad(left, red, green, blue);
    left->x0 = -0x8e;
    left->y0 = -0x6a;
    left->x1 = -0x8e;
    left->y1 = 0x6d;
    left->x2 = -0x86;
    left->y2 = -0x62;
    left->x3 = -0x86;
    left->y3 = 0x65;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    _actor548100InitGlowQuad(right, red, green, blue);
    right->x0 = -0x53;
    right->y0 = -0x6a;
    right->x1 = -0x53;
    right->y1 = 0x6d;
    right->x2 = -0x5b;
    right->y2 = -0x62;
    right->x3 = -0x5b;
    right->y3 = 0x65;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    _actor548100InitGlowQuad(bottom, red, green, blue);
    bottom->x0 = -0x8e;
    bottom->y0 = 0x6d;
    bottom->x1 = -0x53;
    bottom->y1 = 0x6d;
    bottom->x2 = -0x86;
    bottom->y2 = 0x65;
    bottom->x3 = -0x5b;
    bottom->y3 = 0x65;
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_WIRES], bottom);
}

/// Sets wire states for the distance current has travelled along a panel leg.
///
/// `routeId` is 1..9; `progress` is 0..the leg's measured dominant-axis pixel
/// length. Wires behind the front carry FLOW, the crossing wire stores partial
/// flow from its entry node, and later wires remain STOP. `stopNode`'s low byte
/// ends the walk at that node, or zero walks the whole route. Gaps add no wire.
/// Requires initialized graph geometry and a frame's initial wire-state reset.
static void _actor548100SetLegFlowProgress(s32 routeId, s32 stopNode, s16 progress)
{
    const u8* cursor;
    const u8* routeStart;
    u8        previousNode;
    s32       edgeIndex;
    s32       wireEnd;
    s32       wireStart;

    wireEnd      = 0;
    routeStart   = _gActor548100Routes[routeId];
    wireStart    = wireEnd;
    previousNode = routeStart[0];
    cursor       = routeStart + 1;
    while (*cursor != ACTOR_548100_ROUTE_END) {

        if (*cursor != ACTOR_548100_ROUTE_GAP) {
            edgeIndex = _gActor548100WireIndexByNodePair[*cursor + previousNode * ACTOR_548100_NODE_KEY_STRIDE];
            wireEnd  += _gActor548100PanelWires[edgeIndex].length;
            if (progress >= wireEnd) {
                _gActor548100PanelWires[edgeIndex].state = ACTOR_548100_EDGE_STATE_FLOW;
            } else if (wireStart < progress) {
                if (_gActor548100PanelWires[edgeIndex].nodeA == previousNode) {
                    _gActor548100PanelWires[edgeIndex].state = ACTOR_548100_EDGE_STATE_FLOW_FROM_A;
                } else {
                    _gActor548100PanelWires[edgeIndex].state = ACTOR_548100_EDGE_STATE_FLOW_FROM_B;
                }
                _gActor548100PanelWires[edgeIndex].flowLength = progress - wireStart;
            } else {
                _gActor548100PanelWires[edgeIndex].state = ACTOR_548100_EDGE_STATE_STOP;
            }
            wireStart = wireEnd;
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 0);
        } else {
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 1);
        }
        if (*cursor == (stopNode & ACTOR_548100_NODE_ID_MASK)) {
            break;
        }
        cursor++;
    }
}

/// Resets the authored wire table to STOP or ABSENT for the current wiring.
///
/// Stage 2 hides first-only wires; every other stage hides second-only wires.
/// Reads the stage once. The writable authored table ends at nodeA zero; each
/// preceding record becomes STOP unless its layout excludes the selected wiring.
/// Preserves the terminator, geometry and stored partial-flow distances.
static inline void _actor548100ResetWireStatesForStage(void)
{
    _Actor548100Edge* edge;

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == ACTOR_548100_WIRING_SECOND) {
        for (edge = _gActor548100PanelWires; edge->nodeA != 0; edge++) {
            if (edge->layout == ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY) {
                edge->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                edge->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    } else {
        for (edge = _gActor548100PanelWires; edge->nodeA != 0; edge++) {
            if (edge->layout == ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY) {
                edge->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                edge->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    }
}

/// Initializes the panel's node-pair lookup and dominant-axis wire geometry.
///
/// Authored wire endpoints use node ids 1..80 and index the diagram points. The lookup has
/// a 100-node stride; wire indices are 0..90, with nodeA zero ending the table.
/// Coordinates use screen pixels relative to (158, 118), and length excludes
/// one pixel at each end. Resets wire flow for the current wiring afterward.
static void _actor548100InitWireGraph(void)
{
    _Actor548100Edge* edge;
    const DVECTOR*    pointA;
    const DVECTOR*    pointB;
    s32               nodeAX;
    s32               nodeBX;
    s32               nodeAY;
    s32               nodeBY;
    s32               horizontalSpan;
    u8                edgeIndex;

    // Index each authored wire in both directions and derive its pixel span.
    edgeIndex = 0;
    for (edge = _gActor548100PanelWires; edge->nodeA != 0; edge++, edgeIndex++) {
        _gActor548100WireIndexByNodePair[edge->nodeB + edge->nodeA * ACTOR_548100_NODE_KEY_STRIDE] = edgeIndex;
        _gActor548100WireIndexByNodePair[edge->nodeA + edge->nodeB * ACTOR_548100_NODE_KEY_STRIDE] = edgeIndex;
        pointA                                                                                     = &_gActor548100DiagramNodes[edge->nodeA];
        pointB                                                                                     = &_gActor548100DiagramNodes[edge->nodeB];
        nodeAX                                                                                     = pointA->vx - ACTOR_548100_WIRE_ORIGIN_X;
        nodeBX                                                                                     = pointB->vx - ACTOR_548100_WIRE_ORIGIN_X;
        horizontalSpan                                                                             = nodeAX - nodeBX;
        nodeBY                                                                                     = pointB->vy - ACTOR_548100_WIRE_ORIGIN_Y;
        nodeAY                                                                                     = pointA->vy - ACTOR_548100_WIRE_ORIGIN_Y;
        if (horizontalSpan < 0) {
            horizontalSpan = nodeBX - nodeAX;
        }
        if (ABS(nodeAY - nodeBY) < horizontalSpan) {
            edge->length   = ABS(nodeAX - nodeBX) - 2;
            edge->vertical = 0;
            edge->coordA   = nodeAX;
            edge->coordB   = nodeBX;
        } else {
            edge->length   = ABS(nodeAY - nodeBY) - 2;
            edge->vertical = 1;
            edge->coordA   = nodeAY;
            edge->coordB   = nodeBY;
        }
    }
    _actor548100ResetWireStatesForStage();
}

/// Overlays a filled socket or thrown-switch picture on the panel at matching pixels.
///
/// The borrowed rectangle uses top-left-origin screen pixels for both the
/// source texels and destination corners. Appends one unmodulated 15-bit
/// textured quad using the panel's changed-picture page at VRAM (384, 256).
static void _actor548100DrawPanelPiece(const _Actor548100TexRect* rect)
{
    POLY_FT4* quad;
    s32       left;
    s32       top;
    s32       right;
    s32       bottom;

    left           = rect->left;
    right          = rect->right;
    top            = rect->top;
    bottom         = rect->bottom;
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    SetPolyFT4(quad);
    quad->x0    = left - ACTOR_548100_SCREEN_CENTER_X;
    quad->y0    = top - ACTOR_548100_SCREEN_CENTER_Y;
    quad->x1    = right - ACTOR_548100_SCREEN_CENTER_X;
    quad->y1    = top - ACTOR_548100_SCREEN_CENTER_Y;
    quad->x2    = left - ACTOR_548100_SCREEN_CENTER_X;
    quad->y2    = bottom - ACTOR_548100_SCREEN_CENTER_Y;
    quad->x3    = right - ACTOR_548100_SCREEN_CENTER_X;
    quad->y3    = bottom - ACTOR_548100_SCREEN_CENTER_Y;
    quad->u0    = left;
    quad->v0    = top;
    quad->u1    = right;
    quad->v1    = top;
    quad->u2    = left;
    quad->v2    = bottom;
    quad->u3    = right;
    quad->v3    = bottom;
    quad->tpage = ACTOR_548100_PANEL_TEXTURE_PAGE;
    setShadeTex(quad, 1);
    addPrim(&gGpuCurrentOt[ACTOR_548100_OT_PANEL_PIECES], quad);
}

/// Runs the panel's action cursor task: reset both prompt slots, then update them.
///
/// The task's state must be 0 or 1. The reset handler advances state once;
/// subsequent frames use the shared cursor movement and drawing implementation.
static void _actor548100PromptTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    states[task->state](task);
}

/// Queues a battery item for the selected socket after the item menu closes.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; `messageId` and the zero second payload
/// are unused. The live task must have initialized panel work. A battery on
/// socket 1..4 is latched even when occupied or switched on: those cases close
/// the menu without a notice so the panel can explain the refusal by caption.
/// An empty, switched-off socket requests the menu's used-item notice. Other
/// items or choices clear the latch and return refusal. Inventory changes later.
static s32 _actor548100UseBattery(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    _Actor548100Work* work = task->work;

    if ((itemId == ACTOR_548100_BATTERY_FIRST_ITEM || itemId == ACTOR_548100_BATTERY_SECOND_ITEM) && ((u16)work->choice - 1) < (u32)ACTOR_548100_SOCKET_COUNT) {
        work->usedItem = itemId;
        if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) != 0 || gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            return ROOM_KEY_ITEM_USE_NO_NOTICE;
        }
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    work->usedItem = 0;
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Runs one mine power panel state, selects its circuit and draws the panel.
///
/// Task state indexes the eleven handlers, 0..10. Initialization supplies panel
/// work and a cursor task kept in spawnArg2; later states require both to remain
/// live until exit requests completion. The handler table is copied by value
/// before dispatch. Circuit selection and drawing also run on the exit frame.
static void _actor548100PanelTask(Task* task)
{
    TaskFuncTable11 states;

    states = _gActor548100PanelStateHandlers;
    states.funcs[task->state](task);
    _actor548100SelectCircuit();
    _actor548100PostStateHook(task);
    _actor548100DrawPanel(task);
}

/// Marks every hotspot containing the cursor and returns the first choice, or zero.
///
/// Coordinates are signed pixels from the screen centre. All four rectangle
/// edges are inclusive, and overlapping entries are all marked. `hotspots`
/// borrows a writable table ending at `ACTION_PROMPT_HOTSPOT_END`; the sentinel
/// is not modified. A hit whose choice is zero leaves later choices eligible.
static s32 _actionPromptHitTestFirstChoice(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)
{
    s32 firstChoice;

    firstChoice = 0;
    while (hotspots->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((cursorX >= hotspots->x) && ((hotspots->x + hotspots->w) >= cursorX) && (cursorY >= hotspots->y) && ((hotspots->y + hotspots->h) >= cursorY)) {
            hotspots->hit = 1;
            if (firstChoice == 0) {
                firstChoice = hotspots->id;
            }
        } else {
            hotspots->hit = 0;
        }
        hotspots++;
    }
    return firstChoice;
}

/// Scales three authored colour bytes into borrowed output bytes, in RGB order.
///
/// The base components are reread after each preceding output store, preserving
/// aliasing. All six pointers must address live readable or writable bytes as
/// appropriate; output pointers may alias each other or the base bytes. The
/// signed pulse uses 32 for authored brightness and normally cycles 16..32.
/// Signed division truncates toward zero; signed-byte stores retain the low
/// eight RGB bits.
static inline void _actor548100ScaleWireColor(s16 pulse, const u8* baseRed, const u8* baseGreen, const u8* baseBlue,
                                              s8* red, s8* green, s8* blue)
{
    *red   = *baseRed * pulse / ACTOR_548100_COLOR_PULSE_ONE;
    *green = *baseGreen * pulse / ACTOR_548100_COLOR_PULSE_ONE;
    *blue  = *baseBlue * pulse / ACTOR_548100_COLOR_PULSE_ONE;
}

/// Scales the panel's flow colour by its current pulse level.
///
/// `pulse` uses 32 for authored brightness; the panel cycles through 16..32.
/// Each borrowed output points to a writable signed byte holding the low eight
/// RGB bits. Signed division truncates toward zero even for negative pulses;
/// outputs are written red, green, blue, rereading each base component in turn.
static void _actor548100ScaleFlowColor(s16 pulse, s8* red, s8* green, s8* blue)
{
    _actor548100ScaleWireColor(pulse, &_gActor548100FlowBaseRed, &_gActor548100FlowBaseGreen, &_gActor548100FlowBaseBlue, red, green, blue);
}

/// Scales the panel's stopped colour by its current pulse level.
///
/// `pulse` uses 32 for authored brightness; the panel cycles through 16..32.
/// Each borrowed output points to a writable signed byte holding the low eight
/// RGB bits. Signed division truncates toward zero even for negative pulses;
/// outputs are written red, green, blue, rereading each base component in turn.
static void _actor548100ScaleStopColor(s16 pulse, s8* red, s8* green, s8* blue)
{
    _actor548100ScaleWireColor(pulse, &_gActor548100StopBaseRed, &_gActor548100StopBaseGreen, &_gActor548100StopBaseBlue, red, green, blue);
}

/// Scales the panel's short-circuit colour by its current pulse level.
///
/// `pulse` uses 32 for authored brightness; the panel cycles through 16..32.
/// Each borrowed output points to a writable signed byte holding the low eight
/// RGB bits. Signed division truncates toward zero even for negative pulses;
/// outputs are written red, green, blue, rereading each base component in turn.
static void _actor548100ScaleShortColor(s16 pulse, s8* red, s8* green, s8* blue)
{
    _actor548100ScaleWireColor(pulse, &_gActor548100ShortBaseRed, &_gActor548100ShortBaseGreen, &_gActor548100ShortBaseBlueStorage[0], red, green, blue);
}

/// Marks a completed panel leg FLOW, or SHORT when it stops at another battery's route.
///
/// `routeId` is 1..9; `stopNode` is a node on that route, or zero for the whole
/// route. Gaps move the previous node without touching a wire. Requires an
/// initialized node-pair matrix and the frame's initial wire-state reset.
static void _actor548100SetLegComplete(s32 routeId, u8 stopNode)
{
    const u8* cursor;
    const u8* routeStart;
    u8        previousNode;
    u8        currentNode;
    u8        edgeIndex;
    u8        wireState;

    wireState = ACTOR_548100_EDGE_STATE_FLOW;
    if (stopNode != 0) {
        wireState = ACTOR_548100_EDGE_STATE_SHORT;
    }
    routeStart   = _gActor548100Routes[routeId];
    previousNode = routeStart[0];
    cursor       = routeStart + 1;
    while (*cursor != ACTOR_548100_ROUTE_END) {
        currentNode = *cursor;
        if (currentNode != ACTOR_548100_ROUTE_GAP) {
            edgeIndex                                = _gActor548100WireIndexByNodePair[currentNode + previousNode * ACTOR_548100_NODE_KEY_STRIDE];
            _gActor548100PanelWires[edgeIndex].state = wireState;
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 0);
        } else {
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 1);
        }
        if (*cursor++ == stopNode) {
            break;
        }
    }
}

/// Draws every authored wire in its current stopped, flowing or shorted state.
///
/// nodeA zero terminates the wire table. Geometry, scaled RGB bytes, draw area
/// and blend mode must already be ready; absent wires emit no packets.
static void _actor548100DrawWires(void)
{
    const _Actor548100Edge* edge;

    for (edge = _gActor548100PanelWires; edge->nodeA != 0; edge++) {
        _actor548100DrawEdge(edge);
    }
}

/// Clears all wire flow for the current panel wiring before applying a circuit.
///
/// Stage 2 omits first-wiring-only wires; every other stage omits second-only
/// wires. Present wires become STOP, omitted wires ABSENT. nodeA zero ends the
/// writable table; geometry and partial-flow distances are retained.
static void _actor548100ResetWireStates(void)
{
    _actor548100ResetWireStatesForStage();
}

/// Measures a current leg in dominant-axis pixels, excluding each wire's end margins.
///
/// `routeId` is 1..9, or zero for an unused leg, which returns 1 so downstream
/// progress scaling has a nonzero divisor. `stopNode` is a node on that route,
/// or zero to measure the whole route. Gaps move to another node without adding
/// a wire. The node-pair matrix and wire lengths must already be initialized.
static s32 _actor548100MeasureLeg(s32 routeId, u8 stopNode)
{
    const u8* cursor;
    const u8* routeStart;
    u8        previousNode;
    s32       length;

    if (routeId == ACTOR_548100_ROUTE_UNUSED) {
        return 1;
    }
    routeStart   = _gActor548100Routes[routeId];
    length       = 0;
    previousNode = routeStart[0];
    cursor       = routeStart + 1;
    while (*cursor != ACTOR_548100_ROUTE_END) {
        if (*cursor != ACTOR_548100_ROUTE_GAP) {
            length += _gActor548100PanelWires[_gActor548100WireIndexByNodePair[*cursor + previousNode * ACTOR_548100_NODE_KEY_STRIDE]].length;
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 0);
        } else {
            ACTOR_548100_READ_ROUTE_NODE(cursor, previousNode, 1);
        }
        if (*cursor++ == stopNode) {
            break;
        }
    }
    return length;
}

#undef ACTOR_548100_READ_ROUTE_NODE

/// Arms the first panel cursor at screen centre and enters hotspot scanning.
///
/// State 1 sets the shared cursor to AIM speed and IDLE mode, clears its signed
/// screen-pixel position and advances the panel task to state 2.
static void _actor548100ArmCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + (ACTOR_548100_STATE_SCAN_HOTSPOTS - ACTOR_548100_STATE_ARM_CURSOR);
}

/// Opens the selected panel hotspot's commands at the cursor's screen position.
///
/// State 3 requires panel work with the selected hotspot's prompt kind. Hides
/// and stops the first cursor, opens the command menu and enters state 4 to
/// handle the confirmed choice or battery item use.
static void _actor548100OpenHotspotCommands(Task* task)
{
    ActionPrompt*     prompt = gActionPromptSlots;
    _Actor548100Work* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = ACTOR_548100_STATE_HANDLE_CHOICE;
}

/// Restores room presentation and input after closing the mine power panel.
///
/// Requires the live player/model, session, live save and this panel's menu hold.
/// Resumes control and automatic drawing before delaying manual interaction for
/// ten eligible direction updates. Releases one menu hold, clears the event,
/// HUD and cutscene gates, and selects saved refuge view 3. The caller owns
/// cursor and panel teardown; this neither loads the view nor destroys tasks.
static inline void _actor548100RestoreRoomPlay(void)
{
    enum {
        ACTOR_548100_INTERACTION_REARM_UPDATES = 10,
        ACTOR_548100_REFUGE_VIEW               = 3,
        ACTOR_548100_EVENT_IDLE                = 0,
    };

    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    gDirectionInteractionRearmUpdates = ACTOR_548100_INTERACTION_REARM_UPDATES;
    displayReleaseMenuHold();
    gGameSession->eventState                               = ACTOR_548100_EVENT_IDLE;
    gGameSession->hideHud                                  = false;
    gGameSession->cutsceneHold                             = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACTOR_548100_REFUGE_VIEW;
}

/// Restores room play and requests completion of the mine power panel task.
///
/// State 5 resumes and shows the player, releases the menu hold, restores the
/// HUD and refuge view 3, and rearms interaction after ten input updates.
/// Tears down the cursor task owned through spawnArg2 before requesting result
/// zero on this task. The room's polling caller completes panel teardown.
static void _actor548100ExitPanel(Task* task)
{
    _actor548100RestoreRoomPlay();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Places the battery queued by the item menu in the selected unpowered empty socket.
///
/// State 6 requires work naming socket 1..4 and usedItem 0x120 or 0x12C. Only
/// successful placement moves its collected bit from inventory to the socket;
/// power-on or occupied-socket refusal plays a caption and keeps the item.
/// Every path clears the pending request and returns to hotspot scanning.
static void _actor548100PlaceQueuedBattery(Task* task)
{
    _Actor548100Work* work = task->work;
    s32               socketContents;

    if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == ACTOR_548100_SOCKET_EMPTY) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            capStartSequenceSlot(ACTOR_548100_CAP_COMMAND_SOCKET, CAP_PLAYBACK_IN_PLACE, ACTOR_548100_CAP_SOCKET_POWERED);
        } else {
            socketContents = ACTOR_548100_SOCKET_SECOND_BATTERY;
            if (work->usedItem == ACTOR_548100_BATTERY_FIRST_ITEM) {
                socketContents = ACTOR_548100_SOCKET_FIRST_BATTERY;
            }
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
            gameFlagSetNibble(ACTOR_548100_SOCKET_FLAG(work->choice), socketContents);
            inventoryClearCollectedBit(work->usedItem);
        }
    } else {
        capStartSequenceSlot(ACTOR_548100_CAP_COMMAND_SOCKET, CAP_PLAYBACK_IN_PLACE, ACTOR_548100_CAP_SOCKET_OCCUPIED);
    }
    work->usedItem = 0;
    task->state    = ACTOR_548100_STATE_SCAN_HOTSPOTS;
}

/// Waits for a socket battery's pickup caption and empties the socket if taken.
///
/// State 7 requires panel work naming the selected socket and return pickup
/// object 4 or 5. After CAP becomes idle, object state 2 confirms collection;
/// only that case clears the socket, records the retrieval flag and plays its
/// sound. Both acceptance and refusal return to hotspot scanning.
static void _actor548100WaitBatteryReturn(Task* task)
{
    enum { ACTOR_548100_BATTERY_PICKUP_COLLECTED = 2 };

    _Actor548100Work* work = task->work;

    if (capIsBusy() == 0) {
        if (areaGetCurrentObjectState(work->pickupObject) == ACTOR_548100_BATTERY_PICKUP_COLLECTED) {
            // The player took the battery back: empty the socket.
            gameFlagSetNibble(ACTOR_548100_SOCKET_FLAG(work->choice), 0);
            gameFlagSetNibble(GAME_FLAG_110, 1);
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
        }
        task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
    }
}

/// Advances the powered panel's current fronts and finishes the switch-on animation.
///
/// State 9 requires measured positive leg lengths and a selected circuit.
/// Progress is stored in signed halfwords, in diagram pixels, and rises four
/// per tick. The shorter battery leg follows the longer leg's distance ratio.
/// Completion waits until longProgress exceeds its length and the third leg
/// has reached its end; equality alone waits one more tick. Stops the current
/// loop and either starts the powered-door caption or returns to scanning.
/// The completion tick retains the previous shortProgress rather than rescaling.
static void _actor548100AnimateCircuitFlow(Task* task)
{
    /// Current-front advance per switch-on tick, in dominant-axis diagram pixels.
    ///
    /// Route lengths sum each wire's dominant-axis pixel span less its two end
    /// margins. The long and third legs advance by this amount; the short leg is
    /// scaled from the long leg's progress. Progress narrows to s16 before
    /// completion tests and clamping to the measured route lengths.
    enum { ACTOR_548100_FLOW_SPEED = 4 };

    enum { ACTOR_548100_CAP_COMMAND_CIRCUIT_COMPLETE = 12 };
    _Actor548100Work* work = task->work;

    work->thirdProgress += ACTOR_548100_FLOW_SPEED;
    work->longProgress  += ACTOR_548100_FLOW_SPEED;
    if (work->thirdLength < work->thirdProgress) {
        work->thirdProgress = work->thirdLength;
    }
    if (work->longProgress > work->longLength) {
        work->longProgress = work->longLength;
        if (work->thirdProgress == work->thirdLength) {
            sndEvtRequestScriptStop(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            if (_gActor548100CurrentCircuit->powersDoor != 0) {
                sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_COMPLETE, 0, 0);
                capRunCommand(ACTOR_548100_CAP_COMMAND_CIRCUIT_COMPLETE, CAP_PLAYBACK_IN_PLACE);
                task->state = ACTOR_548100_STATE_WAIT_COMPLETION_CAPTION;
            } else {
                task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
            }
            return;
        }
    }
    work->shortProgress = work->shortLength * work->longProgress / work->longLength;
}

/// Returns to hotspot scanning when the powered-circuit completion caption ends.
///
/// State 10 follows the flow animation's completion caption. Leaves the state
/// unchanged while CAP is busy.
static void _actor548100WaitCompletionCaption(Task* task)
{
    if (capIsBusy() == 0) {
        task->state = ACTOR_548100_STATE_SCAN_HOTSPOTS;
    }
}

#include "../../shared/room/action_prompt_reset.inc.c"
