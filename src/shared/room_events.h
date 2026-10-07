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
 *   RoomEventMsg      gRoomEventMsg       the latched message
 *   RoomEventReq      gRoomEventReq       the latched request (gate and task)
 *   u8                gRoomEventActive    raised while an event runs
 *   TaskDesc          gRoomEventTaskDesc  the event task the gate spawns
 *   RoomEventMsg      gRoomEventStagedMsg the staged task's latched message
 *   RoomLatchedEvent  gRoomEventLatched   the latched event (staged task)
 *   RoomFadeStorage   gRoomEventFade      the staged task's fade record (`fade`)
 *   RoomDeparture     gRoomDeparture      the departure the handler staged
 *
 * Some rooms' copies of the flag, the request, the latched event or the
 * departure occupy more bytes than that record, and two rooms keep the fade
 * as a bare `ScreenFade`. The shared code reaches the five records through
 * the bindings below. A room whose symbol is the wider object, or whose fade
 * is the bare record, defines the binding before including this header.
 *
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

#include "main/task_types.h"

#include "rooms/room.h"
#include "rooms/room_common.h"

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
void       roomEventTask(Task* task);
void       roomEventStagedTask(Task* arg0);
void       roomDepartureTask(Task* arg0);

#endif /* SRC_SHARED_ROOM_EVENTS_H */
