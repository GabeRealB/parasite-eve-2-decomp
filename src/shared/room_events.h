/* Room events: the gate a room's message handler asks whether an event may run,
 * which latches it and spawns the event task, and the two event tasks rooms
 * use - one that runs the event's caption command and two sounds before the
 * room change, and a staged one that waits on each step - and the departure
 * task, which turns the player, plays a sound and changes room.
 *
 * Include this header in the prologue and room_event_gate, room_event_task,
 * room_event_staged_task or room_event_departure_task (each with the .inc.c
 * suffix) at the position of that
 * function; a package includes only the ones it carries. The functions have
 * external linkage, since some rooms call them from another of their files.
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
 * Some rooms keep the flag, the request, the latched event, the departure or
 * the fade inside a larger object, or the fade on its own. The code reaches
 * those five through the macros below, which name the plain objects; such a
 * room defines the one it needs before including this header, naming where
 * the value sits.
 */

#ifndef SRC_SHARED_ROOM_EVENTS_H
#define SRC_SHARED_ROOM_EVENTS_H

#include "main/task_types.h"

#include "rooms/room.h"
#include "rooms/room_common.h"

#ifndef ROOM_EVENT_ACTIVE
#define ROOM_EVENT_ACTIVE gRoomEventActive
#endif
#ifndef ROOM_EVENT_REQ
#define ROOM_EVENT_REQ gRoomEventReq
#endif
#ifndef ROOM_EVENT_LATCHED
#define ROOM_EVENT_LATCHED gRoomEventLatched
#endif
#ifndef ROOM_DEPARTURE
#define ROOM_DEPARTURE gRoomDeparture
#endif
#ifndef ROOM_EVENT_FADE
#define ROOM_EVENT_FADE gRoomEventFade.fade
#endif

s32  roomEventGate(RoomEventReq* req, RoomEventMsg* msg);
void roomEventTask(Task* task);
void roomEventStagedTask(Task* arg0);
void roomDepartureTask(Task* arg0);

#endif /* SRC_SHARED_ROOM_EVENTS_H */
