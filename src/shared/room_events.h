/* Room events: the gate a room's message handler asks whether an event may run,
 * which latches it and spawns the event task, and the two event tasks rooms
 * use - one that runs the event's caption command and two sounds before the
 * room change, and a staged one that waits on each step.
 *
 * Include this header in the prologue and room_event_gate, room_event_task or
 * room_event_staged_task (each with the .inc.c suffix) at the position of that
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
 *   RoomLatchedEvent  gRoomEventLatched   the latched event (staged task)
 *   RoomFadeStorage   gRoomEventFade      the staged task's fade
 */

#ifndef SRC_SHARED_ROOM_EVENTS_H
#define SRC_SHARED_ROOM_EVENTS_H

#include "main/task_types.h"

#include "rooms/room_common.h"

s32  roomEventGate(RoomEventReq* req, RoomEventMsg* msg);
void roomEventTask(Task* task);
void roomEventStagedTask(Task* arg0);

#endif /* SRC_SHARED_ROOM_EVENTS_H */
