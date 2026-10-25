/* Room events: the gate a room's message handler asks whether an event may run,
 * which latches it and spawns the event task, and the two event tasks rooms
 * use - one that runs the event's caption command and two sounds before the
 * room change, and a staged one that waits on each step - and the departure
 * task, which turns the player, plays a sound and changes room.
 *
 * Include this header in the prologue and room_event_gate, room_event_task,
 * room_event_staged_task or room_event_departure_task (each with the .inc.c
 * suffix) at the position of that
 * function; a package includes only the ones it carries. The gate is static
 * in each carrier; the event and departure tasks have external linkage,
 * since some rooms call them from another of their files.
 *
 * The event's state belongs to the room, which declares and defines it at its
 * own positions under these names - declaring it here would move it, since bss
 * is laid out in first-declaration order:
 *
 *   RoomEventMsg      _gRoomEventMsg      the TU-local latched message
 *                     gRoomEventMsg       the cross-TU latched message
 *   RoomEventReq      gRoomEventReq       the latched request (gate and task)
 *   u8                gRoomEventActive    raised while an event runs
 *   TaskDesc          _gRoomEventTaskDesc the TU-local event spawn recipe
 *                     gRoomEventTaskDesc  the cross-TU event spawn recipe
 *   RoomEventMsg      _gRoomEventStagedMsg the TU-local staged destination
 *                     gRoomEventStagedDestination the cross-TU staged destination
 *   RoomLatchedEvent  gRoomEventLatched   the latched event (staged task)
 *   RoomFadeStorage   gRoomEventFade      the staged task's fade record (`fade`)
 *   RoomDeparture     gRoomDeparture      the departure the handler staged
 *
 * The destination snapshot is private to each carrier unless another room
 * translation unit defines its storage. Those overlays declare `gRoomEventMsg` in their
 * private headers and bind `ROOM_EVENT_DESTINATION` there; local carriers define
 * `static RoomEventMsg _gRoomEventMsg` before including the fragments.
 *
 * Some rooms' copies of the flag, the request, the latched event or the
 * departure occupy more bytes than that record, and two rooms keep the fade
 * as a bare `ScreenFade`. The shared code reaches these records through
 * the bindings below. A room whose symbol is the wider object, or whose fade
 * is the bare record, defines the binding before including this header.
 *
 *   ROOM_EVENT_DESTINATION lvalue `RoomEventMsg`. Default `_gRoomEventMsg`.
 *                       Cross-TU storage binds `gRoomEventMsg`; the gate copies
 *                       the entire eight-byte record and the task reads it
 *                       until requesting reload.
 *   ROOM_EVENT_STAGED_DESTINATION lvalue `RoomEventMsg`. Default `_gRoomEventStagedMsg`.
 *                       Cross-TU storage binds `gRoomEventStagedDestination`.
 *                       The complete snapshot stays live through staged reload.
 *   ROOM_EVENT_ACTIVE   lvalue `u8`. Default `gRoomEventActive`, the byte
 *                       itself. `RoomEventActiveBytes` rooms bind
 *                       `.eventStarted`; `RoomEventStartStorage` rooms
 *                       bind `.eventStarted`.
 *   ROOM_EVENT_REQ      lvalue `RoomEventReq`. Default `gRoomEventReq`.
 *                       `RoomEventReqStorage` rooms bind `.request`.
 *   ROOM_EVENT_LATCHED  lvalue `RoomLatchedEvent`. Default `gRoomEventLatched`.
 *                       `RoomLatchedEventStorage` rooms bind `.event`.
 *   ROOM_DEPARTURE      lvalue `RoomDeparture`. Default `gRoomDeparture`.
 *                       shelter_b2_main_corridor's symbol is sixteen bytes
 *                       and binds `.departure`.
 *   ROOM_EVENT_FADE     lvalue `ScreenFade`. Default `gRoomEventFade.fade`,
 *                       the record inside `RoomFadeStorage`. A room whose
 *                       symbol is the `ScreenFade` itself binds `gRoomEventFade`.
 *
 * Each binding names an object the room defines. The wider objects are
 * `RoomEventActiveBytes`, `RoomEventStartStorage`, `RoomEventReqStorage`
 * and `RoomLatchedEventStorage`.
 */

#ifndef SRC_SHARED_ROOM_EVENTS_H
#define SRC_SHARED_ROOM_EVENTS_H

#include "gameplay/companion_load.h"

#include "rooms/room.h"
#include "rooms/room_common.h"

#ifndef ROOM_EVENT_TASK_DESC
/// Selects the room-owned descriptor for the gate's deferred transition task.
///
/// Bind an addressable `TaskDesc` lvalue, declared before including the gate
/// fragment. Its recipe uses `TASK_BODY_NONE`, `TASK_PRIORITY_SCENE` and
/// `roomEventTask`; its zero metadata is unused. The gate takes its address
/// and selects index 0 with two unused zero payloads after latching the request
/// and destination. The address expression is evaluated once per spawn.
///
/// Spawning reads the descriptor synchronously without retaining its address
/// or invoking the callback. Keep the room's callback code and latched records
/// loaded and unchanged until the deferred transition requests session reload.
/// Allocation failure still leaves the request, destination and flag committed.
///
/// The default names the cross-TU recipe declared in the room's private header.
/// A carrier with a TU-local recipe overrides this binding before including
/// this header and keeps it through `room_event.inc.c`, even when other
/// event storage is shared between room translation units. The binding itself
/// declares no object and allocates no storage.
#define ROOM_EVENT_TASK_DESC (gRoomEventTaskDesc)
#endif

#ifndef ROOM_EVENT_DESTINATION
/// Selects the room-owned destination retained for the gate's deferred transition.
///
/// Requires a writable `RoomEventMsg` lvalue naming stable storage without
/// side effects, declared before the event fragments. The default is the
/// carrier's static `_gRoomEventMsg`; the night factory, night General Store,
/// B1 armory and B3 elevator hall bind their cross-TU `gRoomEventMsg` in private
/// headers included before this header. Writers and readers use the same binding;
/// the macro declares no object and allocates no storage.
///
/// Eligible execution copies the complete eight-byte input before spawning,
/// even if task allocation fails. Queries and bypassed/refused events leave
/// the snapshot unchanged; another eligible execution replaces it. Keep it
/// loaded and unchanged until the task commits its destination and requests
/// reload. Only area, arrival warp and room are committed within the current
/// stage; the remaining message bytes are copied but not interpreted by the
/// task. Area narrows to the saved byte and room keeps its low eight bits.
/// The lvalue is evaluated once per copy and separately for each field read.
#define ROOM_EVENT_DESTINATION (_gRoomEventMsg)
#endif
#ifndef ROOM_EVENT_STAGED_DESTINATION
/// Selects the room-owned destination retained during staged playback for session reload.
///
/// Requires a writable `RoomEventMsg` lvalue naming stable storage without
/// side effects. The carrier declares it before including the event fragments;
/// the default is its static `_gRoomEventStagedMsg`. An overlay-private header
/// may override this binding before including this header to share storage
/// between room translation units. Writers and the staged task use the same
/// binding; the macro declares no object and allocates no storage.
///
/// Eligible execution copies the complete eight-byte reply before spawning;
/// queries leave this snapshot unchanged. Keep it loaded and unchanged through
/// CAP/sound playback until reload is requested. A later accepted event replaces
/// it. The task commits only area, warp and room within the current stage; the
/// other bytes are retained by the copy but are not interpreted by that task.
/// Each copy evaluates the lvalue once; reload evaluates it separately for each
/// of its three field reads.
#define ROOM_EVENT_STAGED_DESTINATION (_gRoomEventStagedMsg)
#endif
#ifndef ROOM_EVENT_ACTIVE
/// Raised while the gate's event runs. The room's `gRoomEventActive` when that symbol is the byte.
#define ROOM_EVENT_ACTIVE gRoomEventActive
#endif
#ifndef ROOM_EVENT_REQ
/// Request the gate latched. The room's `gRoomEventReq` when that symbol is the request itself.
#define ROOM_EVENT_REQ gRoomEventReq
#endif
#ifndef ROOM_EVENT_LATCHED
/// Event the staged task runs. The room's `gRoomEventLatched` when that symbol is the event itself.
#define ROOM_EVENT_LATCHED gRoomEventLatched
#endif
#ifndef ROOM_DEPARTURE
/// Departure the handler staged. The room's `gRoomDeparture` when that symbol is the departure itself.
#define ROOM_DEPARTURE gRoomDeparture
#endif
#ifndef ROOM_EVENT_FADE
/// Fade record passed to task 0x31. The `fade` member of the room's `RoomFadeStorage gRoomEventFade`.
#define ROOM_EVENT_FADE gRoomEventFade.fade
#endif

static s32 _roomEventGate(const RoomEventReq* request, const RoomEventMsg* message);
/// Plays the latched door-event command and sounds, then reloads its destination.
///
/// Requires live room-owned `ROOM_EVENT_REQ` and `ROOM_EVENT_DESTINATION` snapshots that
/// remain unchanged until state 5. State must be 0..5; spawn arguments are unused.
/// Pauses actors and holds the player; each zero sound ID skips that voice wait.
/// The command is started with a display transition, without a separate CAP wait.
/// Commits area/warp/room to the live save and requests session reload, then
/// kills this task. Does not update the gate's start-request indication.
void roomEventTask(Task* task);
/// Plays a latched room scene, optional blackout and sound before reloading.
///
/// Requires stable room-owned `ROOM_EVENT_LATCHED` and `ROOM_EVENT_STAGED_DESTINATION`
/// through state 4; spawn arguments are unused and states are 0..4. Waits for
/// CAP completion before starting a requested 30-frame subtractive fade, then
/// plays the optional stage-relative sound and waits for its resolved voice.
/// The fade borrows `ROOM_EVENT_FADE` and never receives a return request here:
/// session reload discards its task list before replacing room storage. Keep
/// that record loaded and unchanged until teardown. Does not wait for the
/// fade ramp before committing area/warp/room and requesting session reload.
void roomEventStagedTask(Task* task);
/// Turns the player, waits for an optional departure sound and reloads the session.
///
/// Requires a live player task and stable room-owned `ROOM_DEPARTURE` through
/// state 4; spawn arguments are unused and active states are 0..4. Facing uses
/// 4096 units per turn; `ROOM_DEPARTURE_SKIP_FACING` bypasses the turn and wait.
/// Dispatch reads the stack transform synchronously and only its yaw is set.
/// A zero sound ID skips playback. Commits stage/area/warp/room to the live
/// save, requests session reload and kills this task.
void roomEventDepartureTask(Task* task);

#endif /* SRC_SHARED_ROOM_EVENTS_H */
