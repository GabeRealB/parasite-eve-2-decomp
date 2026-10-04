#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
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
#include "../../shared/action_prompt.h"

/// Distance the current advances along a route on each frame of the panel's
/// switch-on animation, in the units of `_Actor548100Edge::length`.
#define ACTOR_548100_FLOW_SPEED 4

/// Game flag of battery socket `socket`, counted from 1 as the socket hotspots
/// are (`_Actor548100Work::choice`).
///
/// The four flags are consecutive and end at
/// `GAME_FLAG_MINE_POWER_PANEL_SOCKET_4`. Each holds 0 while its socket is empty
/// and otherwise which of the two battery key items sits in it: 1 for item
/// 0x120, 2 for item 0x12C.
#define ACTOR_548100_SOCKET_FLAG(socket) (GAME_FLAG_MINE_POWER_PANEL_SOCKET_4 - 4 + (socket))

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
/// measured once, and the three `...Progress` distances then grow by
/// `ACTOR_548100_FLOW_SPEED` a frame until every leg has run its length. The
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

/// The circuit of the current wiring that matches the filled sockets, looked
/// up again every frame; `NULL` until the first lookup.
extern _Actor548100Circuit* D_actor_548100_80135B4C;

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

extern _Actor548100Edge D_actor_548100_801351D0[];
/// Node points `(x, y)`, indexed by node id.
extern DVECTOR D_actor_548100_801358E4[];
/// Route strings, indexed by route id: node ids, 0xFF-escaped, 0-terminated.
extern u8* D_actor_548100_80135B24[];
/// Edge-id matrix keyed `prev * 100 + cur`.
extern u8 D_actor_548100_80135B5C[10000];

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

// Eleven socket masks: empty, four singles, and six pairs. Items 0x120/0x12C
// move between inventory and sockets; pickup slot 6 supplies the second battery
// once, while slots 4/5 return installed batteries. Thus normal puzzle actions
// match a record before the end of this table, which has no sentinel.
extern _Actor548100Circuit D_actor_548100_80135750[11];
extern _Actor548100TexRect D_actor_548100_801357C0[5];

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
extern _Actor548100ColorRow D_actor_548100_801358A8[3];

static void func_actor_548100_80132420(Task* task);
static void func_actor_548100_80132550(Task* task);
static void func_actor_548100_80132684(Task* task);
static void func_actor_548100_80132808(Task* arg0);
static void func_actor_548100_801330EC(void);
static void func_actor_548100_80134400(ActionPromptHotspot* unused);
static s32  func_actor_548100_801348A4(ActionPromptHotspot* table, s16 x, s16 y);
static s32  func_actor_548100_80134CB8(s32 nodeA, u8 nodeB);
static void func_actor_548100_80134D88(Task* task);
static void func_actor_548100_80134DBC(Task* task);
static void func_actor_548100_80134E0C(Task* arg0);
static void func_actor_548100_80134E94(Task* arg0);
static void func_actor_548100_80134F64(Task* arg0);
static void func_actor_548100_80134FEC(Task* arg0);
static void func_actor_548100_80135124(Task* arg0);
static void func_actor_548100_8013461C(_Actor548100TexRect* rect);
static void func_actor_548100_80133BBC(s32 arg0);
static void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b);
static void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos);
static void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3);
static void func_actor_548100_80134AE0(s32 id, u8 stop);
static void func_actor_548100_80134BA8(void);
static void func_actor_548100_80134BF0(void);

extern TaskDesc D_actor_548100_801351B4;
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry    D_actor_548100_801351C0[];
extern ActionPromptHotspot D_actor_548100_801357E8[];
extern _Actor548100Circuit D_actor_548100_801356D8[12];
extern s16                 D_actor_548100_80135B50;
extern u8                  D_actor_548100_80135B52;
extern s8                  D_actor_548100_80135B53;
extern s8                  D_actor_548100_80135B54;
extern s8                  D_actor_548100_80135B55;
extern s8                  D_actor_548100_80135B56;
extern s8                  D_actor_548100_80135B57;
extern s8                  D_actor_548100_80135B58;
extern s8                  D_actor_548100_80135B59;
extern s8                  D_actor_548100_80135B5A;
extern s8                  D_actor_548100_80135B5B;
extern u8                  D_actor_548100_80135884;
extern u8                  D_actor_548100_80135885;
extern u8                  D_actor_548100_80135886;
extern u8                  D_actor_548100_80135887;
extern u8                  D_actor_548100_80135888;
extern u8                  D_actor_548100_80135889;
extern u8                  D_actor_548100_8013588A;
extern u8                  D_actor_548100_8013588B;
extern u8                  D_actor_548100_8013588C[28];

s32  func_actor_548100_80134778(Task*, s32, s32, s32);
void func_actor_548100_80134728(Task*);

static const char D_actor_548100_80131E54[6];
static const char D_actor_548100_80131E5C[5];
static const char D_actor_548100_80131E64[5];
static void       func_actor_548100_801347F8(Task*);

TaskDesc D_actor_548100_801351B4 = { { { TASK_BODY_NONE, 192 } }, func_actor_548100_80134728, { .value = 0 } };

TaskMessageEntry D_actor_548100_801351C0[2] = {
    { 5105, func_actor_548100_80134778 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

_Actor548100Edge D_actor_548100_801351D0[92] = {
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

_Actor548100Circuit D_actor_548100_801356D8[12] = {
    { 0, 0, { { 0, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 1, 0, { { 1, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 2, 0, { { 2, 0 }, { 0, 0 }, { 9, 0 } }, 1, 1 },
    { 3, 0, { { 3, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 4, 0, { { 4, 0 }, { 0, 0 }, { 9, 0 } }, 0, 1 },
    { 1, 2, { { 1, 9 }, { 2, 9 }, { 9, 0 } }, 0, 1 },
    { 1, 3, { { 1, 11 }, { 3, 11 }, { 9, 0 } }, 0, 1 },
    { 1, 4, { { 1, 36 }, { 4, 36 }, { 9, 0 } }, 0, 1 },
    { 2, 3, { { 2, 0 }, { 3, 0 }, { 9, 0 } }, 1, 1 },
    { 2, 4, { { 2, 0 }, { 4, 0 }, { 9, 0 } }, 1, 1 },
    { 3, 4, { { 3, 27 }, { 4, 27 }, { 9, 0 } }, 0, 1 },
    { -1, -1, { { 0, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
};

_Actor548100Circuit D_actor_548100_80135750[11] = {
    { 0, 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 1, 0, { { 5, 0 }, { 0, 0 }, { 0, 0 } }, 1, 0 },
    { 2, 0, { { 6, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 3, 0, { { 7, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0 },
    { 4, 0, { { 8, 0 }, { 0, 0 }, { 0, 0 } }, 0, 1 },
    { 1, 2, { { 5, 9 }, { 6, 9 }, { 0, 0 } }, 0, 0 },
    { 1, 3, { { 5, 13 }, { 7, 13 }, { 0, 0 } }, 0, 0 },
    { 1, 4, { { 5, 0 }, { 8, 0 }, { 0, 0 } }, 1, 1 },
    { 2, 3, { { 6, 18 }, { 7, 18 }, { 0, 0 } }, 0, 0 },
    { 2, 4, { { 6, 11 }, { 8, 11 }, { 0, 0 } }, 0, 0 },
    { 3, 4, { { 7, 22 }, { 8, 22 }, { 0, 0 } }, 0, 0 },
};

_Actor548100TexRect D_actor_548100_801357C0[5] = {
    { 76, 33, 89, 47 },
    { 89, 45, 103, 59 },
    { 76, 57, 89, 71 },
    { 89, 69, 103, 83 },
    { 0, 0, 75, 239 },
};

ActionPromptHotspot D_actor_548100_801357E8[13] = {
    { -84, -87, 14, 14, 1, 0, 0 },
    { -71, -75, 14, 14, 2, 0, 0 },
    { -84, -63, 14, 14, 3, 0, 0 },
    { -71, -51, 14, 14, 4, 0, 0 },
    { -134, 0, 43, 88, 5, 0, 0 },
    { -134, -98, 44, 200, 6, 0, 0 },
    { 63, 42, 44, 38, 9, 0, 0 },
    { -58, -96, 189, 55, 7, 0, 0 },
    { 75, -46, 56, 117, 7, 0, 0 },
    { 75, -46, 56, 117, 7, 0, 0 },
    { -79, -1, 152, 93, 7, 0, 0 },
    { -37, 92, 96, 20, 8, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

u8 D_actor_548100_80135884 = 16;

u8 D_actor_548100_80135885 = 165;

u8 D_actor_548100_80135886 = 98;

u8 D_actor_548100_80135887 = 60;

u8 D_actor_548100_80135888 = 60;

u8 D_actor_548100_80135889 = 16;

u8 D_actor_548100_8013588A = 160;

u8 D_actor_548100_8013588B = 16;

u8 D_actor_548100_8013588C[28] = {
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

_Actor548100ColorRow D_actor_548100_801358A8[3] = {
    { { &D_actor_548100_80135884, &D_actor_548100_80135885, &D_actor_548100_80135886 }, D_actor_548100_80131E64 },
    { { &D_actor_548100_80135887, &D_actor_548100_80135888, &D_actor_548100_80135889 }, D_actor_548100_80131E5C },
    { { &D_actor_548100_8013588A, &D_actor_548100_8013588B, D_actor_548100_8013588C }, D_actor_548100_80131E54 },
};

TaskDesc D_actor_548100_801358D8 = { { { TASK_BODY_NONE, 192 } }, func_actor_548100_801347F8, { .value = 0 } };

DVECTOR D_actor_548100_801358E4[82] = {
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

u8 D_actor_548100_80135A2C[36] = {
    69,
    75,
    255,
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
    0,
    0,
    0,
};

u8 D_actor_548100_80135A50[24] = {
    70,
    76,
    77,
    255,
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
    0,
    0,
    0,
};

u8 D_actor_548100_80135A68[28] = {
    71,
    78,
    255,
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
    0,
    0,
};

u8 D_actor_548100_80135A84[24] = {
    72,
    79,
    80,
    255,
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
    0,
    0,
    0,
};

u8 D_actor_548100_80135A9C[28] = {
    69,
    75,
    255,
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
    0,
    0,
    0,
};

u8 D_actor_548100_80135AB8[28] = {
    70,
    76,
    77,
    255,
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
    0,
};

u8 D_actor_548100_80135AD4[36] = {
    71,
    78,
    255,
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
    0,
};

u8 D_actor_548100_80135AF8[36] = {
    72,
    79,
    80,
    255,
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
    0,
    0,
    0,
    0,
};

u8 D_actor_548100_80135B1C[8] = {
    68,
    1,
    2,
    73,
    74,
    0,
    0,
    0,
};

u8* D_actor_548100_80135B24[10] = {
    NULL,
    D_actor_548100_80135A2C,
    D_actor_548100_80135A50,
    D_actor_548100_80135A68,
    D_actor_548100_80135A84,
    D_actor_548100_80135A9C,
    D_actor_548100_80135AB8,
    D_actor_548100_80135AD4,
    D_actor_548100_80135AF8,
    D_actor_548100_80135B1C,
};

_Actor548100Circuit* D_actor_548100_80135B4C = NULL;

s16 D_actor_548100_80135B50 = 0;

u8 D_actor_548100_80135B52 = 0;

s8 D_actor_548100_80135B53 = 0;

s8 D_actor_548100_80135B54 = 0;

s8 D_actor_548100_80135B55 = 0;

s8 D_actor_548100_80135B56 = 0;

s8 D_actor_548100_80135B57 = 0;

s8 D_actor_548100_80135B58 = 0;

u8 D_actor_548100_80135B5C[10000] = { 0 };

static void func_actor_548100_80132A14(Task* task);
static void func_actor_548100_80132EA0(Task* task);
static void func_actor_548100_80133684(_Actor548100Edge* edge);
static void func_actor_548100_80133F88(void);

#include "../../shared/action_prompt_move_cursors.inc.c"

/// Three prompt-mode labels nothing in the actor reads.
static const char D_actor_548100_80131E54[] = "Short";
static const char D_actor_548100_80131E5C[] = "Stop";
static const char D_actor_548100_80131E64[] = "Flow";

/// State table of the actor's `Task::callback`, `func_actor_548100_801347F8`,
/// one handler per `Task::state`, which that body copies onto its stack before
/// indexing. States 0 and 2 are the spawners, 1 and 3 arm and re-spawn the
/// action prompt, 4 is the `choice` switch and 9 the switch-on animation.
static const TaskFuncTable11 D_actor_548100_80131E6C = { {
    func_actor_548100_80132420,
    func_actor_548100_80134D88,
    func_actor_548100_80132550,
    func_actor_548100_80134DBC,
    func_actor_548100_80132684,
    func_actor_548100_80134E0C,
    func_actor_548100_80134E94,
    func_actor_548100_80134F64,
    func_actor_548100_80132808,
    func_actor_548100_80134FEC,
    func_actor_548100_80135124,
} };

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// State 0 of the actor's callback: allocates the work block, spawns the
/// action-prompt task and initializes the map UI.
static void func_actor_548100_80132420(Task* task)
{
    _Actor548100Work*    work;
    ActionPromptHotspot* rec;
    ActionPromptHotspot* start;

    work = memCalloc(sizeof(_Actor548100Work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = Task_SpawnFromTable(&D_actor_548100_801351B4, 0, 1, 0);
    task->msgTable                                             = D_actor_548100_801351C0;
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    task->state                                               += 1;
    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 0) {
        gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE, 1);
        gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SOCKET_4, 1);
    }
    work->usedItem = 0;
    Display_AcquireRef();
    start = D_actor_548100_801357E8;
    for (rec = start; rec->id != ACTION_PROMPT_HOTSPOT_END; rec++) {
        rec->hit = 0;
    }
    D_actor_548100_80135B50 = 0x10;
    D_actor_548100_80135B52 = 0;
    func_actor_548100_80134400(start);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    Gp_MsgPlayerWeapon(0);
    Gp_MsgPlayer3F3(0);
}

static void func_actor_548100_80132550(Task* task)
{
    ActionPrompt*        prompt = D_80114D28;
    ActionPromptHotspot* hs     = D_actor_548100_801357E8;
    _Actor548100Work*    work   = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    work->choice        = 0;
    if (func_actor_548100_801348A4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

static void func_actor_548100_80132684(Task* task)
{
    _Actor548100Work* work = task->work;
    s32               kind;
    s32               state;
    s32               cmd;

    D_80114D28[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    D_80114D28[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (func_800D4EC0() != 0) {
        switch (work->choice) {
            case 1:
            case 2:
            case 3:
            case 4:
                if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == 0) {
                    Gp_StartCapSlot(6, 0, 0);
                    state = 2;
                } else if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
                    Gp_StartCapSlot(6, 1, 3);
                    state = 2;
                } else {
                    if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == 1) {
                        work->pickupObject = 4;
                        kind               = 2;
                    } else {
                        work->pickupObject = 5;
                        kind               = 5;
                    }
                    Gp_SetCurBit2Flag(work->pickupObject, 1);
                    Gp_StartCapSlot(6, 0, kind);
                    state = 7;
                }
                break;
            case 5:
                Gp_RunCapCmd(5, 0);
                state = 8;
                break;
            case 6:
                cmd = 4;
                goto run;
            case 7:
                cmd = 7;
                goto run;
            case 8:
                cmd = 9;
                goto run;
            case 9:
                cmd = 8;
            run:
                Gp_RunCapCmd(cmd, 0);
                state = 2;
                break;
            default:
                goto def;
        }
    } else {
        state = work->usedItem;
        if (state != 0) {
            state = 6;
        } else {
        def:
            state = 2;
        }
    }
    task->state = state;
}

/// Player pressed the action button on this actor's map marker with the marker
/// route done: measure the route legs the current will run along and hand the
/// actor on to state 9.
static void func_actor_548100_80132808(Task* arg0)
{
    _Actor548100Work* work = arg0->work;
    s32               distA;
    s32               distB;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            if (gameFlagGetNibble(GAME_FLAG_110) != 0) {
                Gp_SetItemSeenBit(0x120, 1);
                Gp_SetItemSeenBit(0x12C, 1);
            }
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 1);
            arg0->state = 9;
            func_actor_548100_801330EC();
            work->thirdLength = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[2].route, D_actor_548100_80135B4C->leg[2].stopNode);
            distA             = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[0].route, D_actor_548100_80135B4C->leg[0].stopNode);
            distB             = func_actor_548100_80134CB8(D_actor_548100_80135B4C->leg[1].route, D_actor_548100_80135B4C->leg[1].stopNode);
            if (distB < distA) {
                work->longLength        = distA;
                work->shortLength       = distB;
                work->longLeg.route     = D_actor_548100_80135B4C->leg[0].route;
                work->shortLeg.route    = D_actor_548100_80135B4C->leg[1].route;
                work->longLeg.stopNode  = D_actor_548100_80135B4C->leg[0].stopNode;
                work->shortLeg.stopNode = D_actor_548100_80135B4C->leg[1].stopNode;
            } else {
                work->longLength        = distB;
                work->shortLength       = distA;
                work->longLeg.route     = D_actor_548100_80135B4C->leg[1].route;
                work->shortLeg.route    = D_actor_548100_80135B4C->leg[0].route;
                work->longLeg.stopNode  = D_actor_548100_80135B4C->leg[1].stopNode;
                work->shortLeg.stopNode = D_actor_548100_80135B4C->leg[0].stopNode;
            }
            work->longProgress  = 0;
            work->shortProgress = 0;
            work->thirdProgress = 0;
            return;
        }
        if (Gp_GetCapEventKey() == 0x15) {
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_SWITCH, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON, 0);
        }
        arg0->state = 2;
    }
}

static void func_actor_548100_80132A14(Task* task)
{
    _Actor548100Work*    work;
    _Actor548100TexRect* rect;
    DR_MODE*             prim;
    _Actor548100Circuit* circuit;
    s32                  i;
    s32                  doorLit;
    s32                  passageLit;

    i    = 0;
    rect = D_actor_548100_801357C0;
    work = task->work;
    for (; i < 4; i++, rect++) {
        if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(i + 1)) != 0) {
            func_actor_548100_8013461C(rect);
        }
    }

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 1);
    prim->code[0] = 0xE100022A;
    addPrim(&gGpuCurrentOt[0x3FD], prim);

    func_actor_548100_80134960(D_actor_548100_80135B50, &D_actor_548100_80135B53, &D_actor_548100_80135B54, &D_actor_548100_80135B55);
    func_actor_548100_801349E0(D_actor_548100_80135B50, &D_actor_548100_80135B56, &D_actor_548100_80135B57, &D_actor_548100_80135B58);
    func_actor_548100_80134A60(D_actor_548100_80135B50, &D_actor_548100_80135B59, &D_actor_548100_80135B5A, &D_actor_548100_80135B5B);
    func_actor_548100_80134BF0();
    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 0);
    gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
        if (task->state != 9) {
            circuit = D_actor_548100_80135B4C;
            if (circuit->leg[0].route != 0) {
                func_actor_548100_80134AE0(circuit->leg[0].route, circuit->leg[0].stopNode);
                circuit = D_actor_548100_80135B4C;
            }
            if (circuit->leg[1].route != 0) {
                func_actor_548100_80134AE0(circuit->leg[1].route, circuit->leg[1].stopNode);
            }
            circuit = D_actor_548100_80135B4C;
            if (circuit->leg[2].route != 0) {
                func_actor_548100_80134AE0(circuit->leg[2].route, circuit->leg[2].stopNode);
            }
            if (D_actor_548100_80135B4C->powersDoor != 0) {
                func_actor_548100_80133BBC(1);
                gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 1);
            }
            if (D_actor_548100_80135B4C->powersPassage != 0) {
                func_actor_548100_80133BBC(2);
                if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
                    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 3);
                } else {
                    gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 2);
                }
            }
        } else {
            circuit    = D_actor_548100_80135B4C;
            doorLit    = 0;
            passageLit = 0;
            if (circuit->leg[2].route != 0) {
                func_actor_548100_801342D8(circuit->leg[2].route, circuit->leg[2].stopNode, work->thirdProgress);
                passageLit = work->thirdProgress == work->thirdLength;
            }
            if (work->longLeg.route != 0) {
                if (work->longProgress != work->longLength) {
                    func_actor_548100_801342D8(work->longLeg.route, work->longLeg.stopNode, work->longProgress);
                } else {
                    func_actor_548100_80134AE0(work->longLeg.route, work->longLeg.stopNode);
                }
            }
            if (work->shortLeg.route != 0) {
                if (work->shortProgress != work->shortLength) {
                    func_actor_548100_801342D8(work->shortLeg.route, work->shortLeg.stopNode, work->shortProgress);
                } else {
                    func_actor_548100_80134AE0(work->shortLeg.route, work->shortLeg.stopNode);
                }
            }
            if (work->longProgress == work->longLength && work->longLeg.stopNode == 0 && work->longLeg.route != 0) {
                doorLit    = D_actor_548100_80135B4C->powersDoor;
                passageLit = passageLit || D_actor_548100_80135B4C->powersPassage;
            }
            if (doorLit != 0) {
                func_actor_548100_80133BBC(1);
            }
            if (passageLit != 0) {
                func_actor_548100_80133BBC(2);
            }
        }
    }

    func_actor_548100_80134BA8();
    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == 0) {
        gameFlagSetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_MINE_GORGE_CAVERN_DOOR_POWERED, 0);
    } else {
        func_actor_548100_8013461C(&D_actor_548100_801357C0[4]);
    }

    if (D_actor_548100_80135B52 == 0) {
        if (++D_actor_548100_80135B50 >= 0x20) {
            D_actor_548100_80135B52 = 1;
        }
    } else if (D_actor_548100_80135B52 == 1) {
        if (--D_actor_548100_80135B50 <= 0x10) {
            D_actor_548100_80135B52 = 0;
        }
    } else if (D_actor_548100_80135B50 > 0) {
        D_actor_548100_80135B50--;
    }
}

/// Per-frame hook the actor's callback runs after the state handler; empty in
/// this actor.
static void func_actor_548100_80132EA0(Task* task)
{
}

#include "../../shared/action_prompt_outline_rect.inc.c"

static void func_actor_548100_801330EC(void)
{
    s32 want;
    s32 have;
    s32 i;

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 1) {
        D_actor_548100_80135B4C = D_actor_548100_801356D8;
    } else {
        D_actor_548100_80135B4C = D_actor_548100_80135750;
    }
    while (D_actor_548100_80135B4C->socketA != -1) {
        want = 0;
        if (D_actor_548100_80135B4C->socketA != 0) {
            want = 1 << (D_actor_548100_80135B4C->socketA - 1);
        }
        if (D_actor_548100_80135B4C->socketB != 0) {
            want |= 1 << (D_actor_548100_80135B4C->socketB - 1);
        }
        have = 0;
        for (i = 0; i < 4; i++) {
            if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(i + 1)) != 0) {
                have |= 1 << i;
            }
        }
        if (want == have) {
            break;
        }
        D_actor_548100_80135B4C++;
    }
}

/// Draws a line between nodes `nodeA` and `nodeB` as a flat `r`/`g`/`b` quad
/// two pixels wide, framed on each side by `POLY_G4` edges fading from black
/// into that colour. The quad lies along whichever axis the line spans further,
/// with the nodes ordered so the lower coordinate comes first.
static void func_actor_548100_80133200(s32 nodeA, s32 nodeB, u8 r, u8 g, u8 b)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      dx;
    s32      tmp;
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
    quad->r0 = r;
    quad->g0 = g;
    quad->b0 = b;
    ax       = D_actor_548100_801358E4[nodeA].vx - 0x9E;
    ay       = D_actor_548100_801358E4[nodeA].vy - 0x76;
    bx       = D_actor_548100_801358E4[nodeB].vx - 0x9E;
    by       = D_actor_548100_801358E4[nodeB].vy - 0x76;
    dx       = ax - bx;
    if (dx < 0) {
        dx = bx - ax;
    }
    if (ABS(ay - by) < dx) {
        if (bx < ax) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax + 1;
        y0 = ay - 1;
        x1 = bx - 1;
        y1 = by - 1;
        x2 = x0;
        y2 = ay + 1;
        x3 = x1;
        y3 = by + 1;
    } else {
        if (by < ay) {
            tmp = ax;
            ax  = bx;
            bx  = tmp;
            tmp = ay;
            ay  = by;
            by  = tmp;
        }
        x0 = ax - 1;
        y0 = ay + 1;
        x1 = ax + 1;
        y1 = y0;
        x2 = bx - 1;
        y2 = by - 1;
        x3 = bx + 1;
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
    addPrim(&gGpuCurrentOt[0x3FC], quad);

    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y1 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x2 - 3;
    left->y1 = y2 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x2;
    left->y3 = y2;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = x1 + 3;
    right->y0 = y1 - 3;
    right->x1 = x3 + 3;
    right->y1 = y3 + 3;
    right->x2 = x1;
    right->y2 = y1;
    right->x3 = x3;
    right->y3 = y3;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = x2 - 3;
    bottom->y0 = y2 + 3;
    bottom->x1 = x3 + 3;
    bottom->y1 = y3 + 3;
    bottom->x2 = x2;
    bottom->y2 = y2;
    bottom->x3 = x3;
    bottom->y3 = y3;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

/// Draws `edge` between its two nodes in the colour its `state` selects
/// (`STOP`, `FLOW` or `SHORT`). The two `FLOW_FROM_*` states (`FLOW_FROM_B`
/// swaps the nodes) split the line `flowLength` along x from the node the
/// current entered at, or along y when `vertical` is set, clipping each half
/// with a `DR_AREA` linked into `gGpuCurrentOt[0x3FC]` and drawing one half
/// per colour.
static void func_actor_548100_80133684(_Actor548100Edge* edge)
{
    RECT     rect;
    DR_AREA* area;
    s32      ax;
    s32      ay;
    s32      bx;
    s32      by;
    s32      pos;
    s32      sign;
    s32      a;
    s32      b;

    a = edge->nodeA;
    b = edge->nodeB;
    switch (edge->state) {
        case ACTOR_548100_EDGE_STATE_SHORT:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B59, D_actor_548100_80135B5A, D_actor_548100_80135B5B);
            break;
        case ACTOR_548100_EDGE_STATE_FLOW:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
            break;
        case ACTOR_548100_EDGE_STATE_STOP:
            func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
            break;
        case ACTOR_548100_EDGE_STATE_FLOW_FROM_B:
            a = edge->nodeB;
            b = edge->nodeA;
        case ACTOR_548100_EDGE_STATE_FLOW_FROM_A:
            area           = gGpuPrimCursor;
            gGpuPrimCursor = area + 1;
            ax             = D_actor_548100_801358E4[a].vx - 0x9E;
            ay             = D_actor_548100_801358E4[a].vy - 0x76;
            bx             = D_actor_548100_801358E4[b].vx - 0x9E;
            by             = D_actor_548100_801358E4[b].vy - 0x76;
            setRECT(&rect, 0, 0, 0x140, 0xF0);
            rect.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[0x3FC], area);
            if (edge->vertical == 0) {
                sign = 1;
                if (bx < ax) {
                    sign = -1;
                }
                pos = ax + sign * edge->flowLength;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ax < bx) {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                } else {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                }
                rect.y += gDisplayState.drawBuffer * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ax < bx) {
                    setRECT(&rect, pos + 0xA0, 0, 0xA0 - pos, 0xF0);
                } else {
                    setRECT(&rect, 0, 0, pos + 0xA0, 0xF0);
                }
            } else {
                sign = 1;
                if (by < ay) {
                    sign = -1;
                }
                pos = ay + sign * edge->flowLength;
                func_actor_548100_80133200(a, b, D_actor_548100_80135B53, D_actor_548100_80135B54, D_actor_548100_80135B55);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ay < by) {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                } else {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                }
                rect.y += gDisplayState.drawBuffer * 0x110;
                SetDrawArea(area, &rect);
                addPrim(&gGpuCurrentOt[0x3FC], area);
                func_actor_548100_80133200(a, b, D_actor_548100_80135B56, D_actor_548100_80135B57, D_actor_548100_80135B58);
                area           = gGpuPrimCursor;
                gGpuPrimCursor = area + 1;
                if (ay < by) {
                    setRECT(&rect, 0, pos + 0x78, 0x140, 0x78 - pos);
                } else {
                    setRECT(&rect, 0, 0, 0x140, pos + 0x78);
                }
            }
            rect.y += gDisplayState.drawBuffer * 0x110;
            SetDrawArea(area, &rect);
            addPrim(&gGpuCurrentOt[0x3FC], area);
            break;
    }
}

/// Draws a translucent flat quad in the (`D_..._80135B53`..`55`) colour at
/// x 0x43..0x68, on row 1 (`arg0 == 1`) or row 2, framed by four `POLY_G4`
/// edges fading from black into that colour, all linked into
/// `gGpuCurrentOt[0x3FC]`.
static void func_actor_548100_80133BBC(s32 arg0)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    s16      x0;
    s16      x1;
    s16      y0;
    s16      y1;
    u8       r1;
    u8       g1;
    u8       b1;
    u8       r2;
    u8       g2;
    u8       b2;
    u8       r3;
    u8       g3;
    u8       b3;
    u8       r4;
    u8       g4;
    u8       b4;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = D_actor_548100_80135B53;
    quad->g0 = D_actor_548100_80135B54;
    quad->b0 = D_actor_548100_80135B55;
    x0       = 0x43;
    if (arg0 == 1) {
        y0 = 0x2E;
        x1 = 0x68;
        y1 = 0x39;
    } else {
        y0 = 0x42;
        x1 = 0x68;
        y1 = 0x4D;
    }
    quad->x0 = x0;
    quad->y0 = y0;
    quad->x1 = x1;
    quad->y1 = y0;
    quad->x2 = x0;
    quad->y2 = y1;
    quad->x3 = x1;
    quad->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], quad);

    r1             = D_actor_548100_80135B53;
    g1             = D_actor_548100_80135B54;
    b1             = D_actor_548100_80135B55;
    top            = gGpuPrimCursor;
    gGpuPrimCursor = top + 1;
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r1;
    top->g2 = g1;
    top->b2 = b1;
    top->r3 = r1;
    top->g3 = g1;
    top->b3 = b1;
    top->x0 = x0 - 3;
    top->y0 = y0 - 3;
    top->x1 = x1 + 3;
    top->y1 = y0 - 3;
    top->x2 = x0;
    top->y2 = y0;
    top->x3 = x1;
    top->y3 = y0;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    r2             = D_actor_548100_80135B53;
    g2             = D_actor_548100_80135B54;
    b2             = D_actor_548100_80135B55;
    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r2;
    left->g2 = g2;
    left->b2 = b2;
    left->r3 = r2;
    left->g3 = g2;
    left->b3 = b2;
    left->x0 = x0 - 3;
    left->y0 = y0 - 3;
    left->x1 = x0 - 3;
    left->y1 = y1 + 3;
    left->x2 = x0;
    left->y2 = y0;
    left->x3 = x0;
    left->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    r3             = D_actor_548100_80135B53;
    g3             = D_actor_548100_80135B54;
    b3             = D_actor_548100_80135B55;
    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r3;
    right->g2 = g3;
    right->b2 = b3;
    right->r3 = r3;
    right->g3 = g3;
    right->b3 = b3;
    right->x0 = x1 + 3;
    right->y0 = y0 - 3;
    right->x1 = x1 + 3;
    right->y1 = y1 + 3;
    right->x2 = x1;
    right->y2 = y0;
    right->x3 = x1;
    right->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    r4             = D_actor_548100_80135B53;
    g4             = D_actor_548100_80135B54;
    b4             = D_actor_548100_80135B55;
    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r4;
    bottom->g2 = g4;
    bottom->b2 = b4;
    bottom->r3 = r4;
    bottom->g3 = g4;
    bottom->b3 = b4;
    bottom->x0 = x0 - 3;
    bottom->y0 = y1 + 3;
    bottom->x1 = x1 + 3;
    bottom->y1 = y1 + 3;
    bottom->x2 = x0;
    bottom->y2 = y1;
    bottom->x3 = x1;
    bottom->y3 = y1;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

/// Draws a translucent flat quad in half the (`D_..._80135B53`..`55`) colour
/// and a gradient border of four `POLY_G4` edges fading from black into
/// that colour, linked into `gGpuCurrentOt[0x3FC]`. The flat quad itself is
/// never linked.
static void func_actor_548100_80133F88(void)
{
    POLY_F4* quad;
    POLY_G4* top;
    POLY_G4* left;
    POLY_G4* right;
    POLY_G4* bottom;
    u8       r;
    u8       g;
    u8       b;

    r = D_actor_548100_80135B53;
    g = D_actor_548100_80135B54;
    b = D_actor_548100_80135B55;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    setSemiTrans(quad, 1);
    quad->r0 = r >> 1;
    quad->g0 = g >> 1;
    quad->b0 = b >> 1;
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
    setPolyG4(top);
    setSemiTrans(top, 1);
    top->r0 = 0;
    top->g0 = 0;
    top->b0 = 0;
    top->r1 = 0;
    top->g1 = 0;
    top->b1 = 0;
    top->r2 = r;
    top->g2 = g;
    top->b2 = b;
    top->r3 = r;
    top->g3 = g;
    top->b3 = b;
    top->x0 = -0x8e;
    top->y0 = -0x6a;
    top->x1 = -0x53;
    top->y1 = -0x6a;
    top->x2 = -0x86;
    top->y2 = -0x62;
    top->x3 = -0x5b;
    top->y3 = -0x62;
    addPrim(&gGpuCurrentOt[0x3FC], top);

    left           = gGpuPrimCursor;
    gGpuPrimCursor = left + 1;
    setPolyG4(left);
    setSemiTrans(left, 1);
    left->r0 = 0;
    left->g0 = 0;
    left->b0 = 0;
    left->r1 = 0;
    left->g1 = 0;
    left->b1 = 0;
    left->r2 = r;
    left->g2 = g;
    left->b2 = b;
    left->r3 = r;
    left->g3 = g;
    left->b3 = b;
    left->x0 = -0x8e;
    left->y0 = -0x6a;
    left->x1 = -0x8e;
    left->y1 = 0x6d;
    left->x2 = -0x86;
    left->y2 = -0x62;
    left->x3 = -0x86;
    left->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], left);

    right          = gGpuPrimCursor;
    gGpuPrimCursor = right + 1;
    setPolyG4(right);
    setSemiTrans(right, 1);
    right->r0 = 0;
    right->g0 = 0;
    right->b0 = 0;
    right->r1 = 0;
    right->g1 = 0;
    right->b1 = 0;
    right->r2 = r;
    right->g2 = g;
    right->b2 = b;
    right->r3 = r;
    right->g3 = g;
    right->b3 = b;
    right->x0 = -0x53;
    right->y0 = -0x6a;
    right->x1 = -0x53;
    right->y1 = 0x6d;
    right->x2 = -0x5b;
    right->y2 = -0x62;
    right->x3 = -0x5b;
    right->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], right);

    bottom         = gGpuPrimCursor;
    gGpuPrimCursor = bottom + 1;
    setPolyG4(bottom);
    setSemiTrans(bottom, 1);
    bottom->r0 = 0;
    bottom->g0 = 0;
    bottom->b0 = 0;
    bottom->r1 = 0;
    bottom->g1 = 0;
    bottom->b1 = 0;
    bottom->r2 = r;
    bottom->g2 = g;
    bottom->b2 = b;
    bottom->r3 = r;
    bottom->g3 = g;
    bottom->b3 = b;
    bottom->x0 = -0x8e;
    bottom->y0 = 0x6d;
    bottom->x1 = -0x53;
    bottom->y1 = 0x6d;
    bottom->x2 = -0x86;
    bottom->y2 = 0x65;
    bottom->x3 = -0x5b;
    bottom->y3 = 0x65;
    addPrim(&gGpuCurrentOt[0x3FC], bottom);
}

static void func_actor_548100_801342D8(s32 id, s32 stop, s16 pos)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 edge;
    s32 total;
    s32 start;

    total = 0;
    head  = D_actor_548100_80135B24[id];
    start = total;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {

        if (*route != 0xFF) {
            edge   = D_actor_548100_80135B5C[*route + prev * 100];
            total += D_actor_548100_801351D0[edge].length;
            if (pos >= total) {
                D_actor_548100_801351D0[edge].state = ACTOR_548100_EDGE_STATE_FLOW;
            } else if (start < pos) {
                if (D_actor_548100_801351D0[edge].nodeA == prev) {
                    D_actor_548100_801351D0[edge].state = ACTOR_548100_EDGE_STATE_FLOW_FROM_A;
                } else {
                    D_actor_548100_801351D0[edge].state = ACTOR_548100_EDGE_STATE_FLOW_FROM_B;
                }
                D_actor_548100_801351D0[edge].flowLength = pos - start;
            } else {
                D_actor_548100_801351D0[edge].state = ACTOR_548100_EDGE_STATE_STOP;
            }
            start = total;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route == (stop & 0xFF)) {
            break;
        }
        route++;
    }
}

/// Build the edge graph's derived state: record every edge's id in the
/// node-pair matrix both ways round, then store its dominant axis in `vertical`
/// (0 horizontal, 1 vertical), that axis's two screen-centred endpoint
/// coordinates in `coordA` / `coordB` and their span less 2 in `length`.
/// Finally reset each edge's `state` for the current stage, as
/// `func_actor_548100_80134BF0` does.
static void func_actor_548100_80134400(ActionPromptHotspot* unused)
{
    _Actor548100Edge* edge;
    _Actor548100Edge* cell;
    DVECTOR*          a;
    DVECTOR*          b;
    s32               ax;
    s32               bx;
    s32               ay;
    s32               by;
    s32               dx;
    u8                i;

    i = 0;
    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++, i++) {
        D_actor_548100_80135B5C[edge->nodeB + edge->nodeA * 100] = i;
        D_actor_548100_80135B5C[edge->nodeA + edge->nodeB * 100] = i;
        a                                                        = &D_actor_548100_801358E4[edge->nodeA];
        b                                                        = &D_actor_548100_801358E4[edge->nodeB];
        ax                                                       = a->vx - 158;
        bx                                                       = b->vx - 158;
        dx                                                       = ax - bx;
        by                                                       = b->vy - 118;
        ay                                                       = a->vy - 118;
        if (dx < 0) {
            dx = bx - ax;
        }
        if (ABS(ay - by) < dx) {
            edge->length   = ABS(ax - bx) - 2;
            edge->vertical = 0;
            edge->coordA   = ax;
            edge->coordB   = bx;
        } else {
            edge->length   = ABS(ay - by) - 2;
            edge->vertical = 1;
            edge->coordA   = ay;
            edge->coordB   = by;
        }
    }
    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->layout == ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY) {
                cell->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                cell->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    } else {
        for (cell = D_actor_548100_801351D0; cell->nodeA != 0; cell++) {
            if (cell->layout == ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY) {
                cell->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                cell->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    }
}

/// Link `rect` into the ordering table as a textured quad, taking the
/// primitive off the `gGpuPrimCursor` bump allocator. The same four numbers are
/// the texture window and, shifted by the screen centre, the quad's screen
/// rectangle -- `u`/`v` are the table's own values and `x`/`y` those values
/// minus 160 and 120, so a record drawn from the origin-centred screen space
/// `_Actor548100TexRect` is authored in lands on the matching part of the
/// texture page. Corner 0 and 2 share the left edge, 1 and 3 the right; the
/// upper corners share the top, the lower pair the bottom.
///
/// `x1`/`x3` are read before `v0`/`v1` -- that order is what puts the four
/// loads in the register file the target uses, and reordering them changes
/// the code without changing the meaning.
static void func_actor_548100_8013461C(_Actor548100TexRect* rect)
{
    POLY_FT4* prim;
    s32       u0;
    s32       v0;
    s32       u1;
    s32       v1;

    u0             = rect->left;
    u1             = rect->right;
    v0             = rect->top;
    v1             = rect->bottom;
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    SetPolyFT4(prim);
    prim->x0    = u0 - 0xA0;
    prim->y0    = v0 - 0x78;
    prim->x1    = u1 - 0xA0;
    prim->y1    = v0 - 0x78;
    prim->x2    = u0 - 0xA0;
    prim->y2    = v1 - 0x78;
    prim->x3    = u1 - 0xA0;
    prim->y3    = v1 - 0x78;
    prim->u0    = u0;
    prim->v0    = v0;
    prim->u1    = u1;
    prim->v1    = v0;
    prim->u2    = u0;
    prim->v2    = v1;
    prim->u3    = u1;
    prim->v3    = v1;
    prim->tpage = 0x116;
    setShadeTex(prim, 1);
    addPrim(&gGpuCurrentOt[0x3FE], prim);
}

/// Callback of the action-prompt task: a two-state dispatcher whose handler
/// table is built on the stack. State 0, `actionPromptReset`, resets
/// both prompt slots; state 1, `actionPromptMoveCursors`, drives the cursor
/// every frame from then on.
void func_actor_548100_80134728(Task* task)
{
    TaskFunc funcs[2] = {
        actionPromptReset,
        actionPromptMoveCursors,
    };

    funcs[task->state](task);
}

s32 func_actor_548100_80134778(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    _Actor548100Work* work = arg0->work;

    if ((arg2 == 0x120 || arg2 == 0x12C) && ((u16)work->choice - 1) < 4U) {
        work->usedItem = arg2;
        if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) != 0 || gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            return 2;
        }
        return 1;
    }
    work->usedItem = 0;
    return 0;
}

static void func_actor_548100_801347F8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_548100_80131E6C;
    fns.funcs[arg0->state](arg0);
    func_actor_548100_801330EC();
    func_actor_548100_80132EA0(arg0);
    func_actor_548100_80132A14(arg0);
}

/// Hit-tests (`x`, `y`) against `table`, raising `hit` on every containing entry
/// and clearing it on the rest. Returns the `id` of the first entry hit, or 0.
static s32 func_actor_548100_801348A4(ActionPromptHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != ACTION_PROMPT_HOTSPOT_END) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            if (hit == 0) {
                hit = table->id;
            }
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

static void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135884 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135885 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135886 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135887 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135888 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135889 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_8013588A * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_8013588B * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_8013588C[0] * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

static void func_actor_548100_80134AE0(s32 id, u8 stop)
{
    u8* route;
    u8* head;
    u8  prev;
    u8  cur;
    u8  edge;
    u8  state;

    state = ACTOR_548100_EDGE_STATE_FLOW;
    if (stop != 0) {
        state = ACTOR_548100_EDGE_STATE_SHORT;
    }
    head  = D_actor_548100_80135B24[id];
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        cur = *route;
        if (cur != 0xFF) {
            edge                                = D_actor_548100_80135B5C[cur + prev * 100];
            D_actor_548100_801351D0[edge].state = state;
            prev                                = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == stop) {
            break;
        }
    }
}

static void func_actor_548100_80134BA8(void)
{
    _Actor548100Edge* edge;

    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
        func_actor_548100_80133684(edge);
    }
}

static void func_actor_548100_80134BF0(void)
{
    _Actor548100Edge* edge;

    if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_STAGE) == 2) {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->layout == ACTOR_548100_EDGE_LAYOUT_FIRST_ONLY) {
                edge->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                edge->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    } else {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->layout == ACTOR_548100_EDGE_LAYOUT_SECOND_ONLY) {
                edge->state = ACTOR_548100_EDGE_STATE_ABSENT;
            } else {
                edge->state = ACTOR_548100_EDGE_STATE_STOP;
            }
        }
    }
}

static s32 func_actor_548100_80134CB8(s32 nodeA, u8 nodeB)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 dist;

    if (nodeA == 0) {
        return 1;
    }
    head  = D_actor_548100_80135B24[nodeA];
    dist  = 0;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        if (*route != 0xFF) {
            dist += D_actor_548100_801351D0[D_actor_548100_80135B5C[*route + prev * 100]].length;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == nodeB) {
            break;
        }
    }
    return dist;
}

/// State 1 of the actor's callback: arms the first action-prompt slot at
/// `ACTION_PROMPT_SPEED_AIM` with the idle cursor, clears its screen position
/// and steps the task on to state 2.
static void func_actor_548100_80134D88(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// State 3 of the actor's callback, entered once a hotspot is picked: clears
/// the prompt's highlight and target, re-spawns the prompt at its current
/// screen position with the picked hotspot's `promptKind`, and moves the task
/// to the `choice` switch in state 4.
static void func_actor_548100_80134DBC(Task* task)
{
    ActionPrompt*     prompt = D_80114D28;
    _Actor548100Work* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

static void func_actor_548100_80134E0C(Task* arg0)
{
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    D_80114D08 = 0xA;
    Display_ReleaseRef();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill(arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

static void func_actor_548100_80134E94(Task* arg0)
{
    _Actor548100Work* work = arg0->work;
    s32               value;

    if (gameFlagGetNibble(ACTOR_548100_SOCKET_FLAG(work->choice)) == 0) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) != 0) {
            Gp_StartCapSlot(6, 0, 1);
        } else {
            value = 2;
            if (work->usedItem == 0x120) {
                value = 1;
            }
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
            gameFlagSetNibble(ACTOR_548100_SOCKET_FLAG(work->choice), value);
            Gp_ClearCollectedBit(work->usedItem);
        }
    } else {
        Gp_StartCapSlot(6, 0, 4);
    }
    work->usedItem = 0;
    arg0->state    = 2;
}

static void func_actor_548100_80134F64(Task* arg0)
{
    _Actor548100Work* work = arg0->work;

    if (Gp_CapBusy() == 0) {
        if (Gp_GetCurBit2Flag(work->pickupObject) == 2) {
            // The player took the battery back: empty the socket.
            gameFlagSetNibble(ACTOR_548100_SOCKET_FLAG(work->choice), 0);
            gameFlagSetNibble(GAME_FLAG_110, 1);
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_BATTERY_SOCKET, 0, 0);
        }
        arg0->state = 2;
    }
}

static void func_actor_548100_80134FEC(Task* arg0)
{
    _Actor548100Work* work = arg0->work;

    work->thirdProgress += ACTOR_548100_FLOW_SPEED;
    work->longProgress  += ACTOR_548100_FLOW_SPEED;
    if (work->thirdLength < work->thirdProgress) {
        work->thirdProgress = work->thirdLength;
    }
    if (work->longProgress > work->longLength) {
        work->longProgress = work->longLength;
        if (work->thirdProgress == work->thirdLength) {
            SndEvt_EnqueueType7(SOUND_MINE_REFUGE_CIRCUIT_CURRENT_LOOP, 1);
            if (D_actor_548100_80135B4C->powersDoor != 0) {
                sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_COMPLETE, 0, 0);
                Gp_RunCapCmd(0xC, 0);
                arg0->state = 0xA;
            } else {
                arg0->state = 2;
            }
            return;
        }
    }
    work->shortProgress = work->shortLength * work->longProgress / work->longLength;
}

static void func_actor_548100_80135124(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        arg0->state = 2;
    }
}

#include "../../shared/action_prompt_reset.inc.c"
