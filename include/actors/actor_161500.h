#ifndef INCLUDE_ACTORS_ACTOR_161500_H
#define INCLUDE_ACTORS_ACTOR_161500_H

#include "main/task_types.h"

/// Bodyless one-shot task recipe for completing the companion's heliport item request.
///
/// Spawn the single descriptor at index 0, priority `TASK_PRIORITY_SCENE`, with unused zero spawn
/// arguments. The room first requires pending request state, item 0x124, a live
/// companion and the nearby action trigger. The task identifies the item, marks
/// the request complete, starts the handover scene and kills itself. Keep this
/// actor, the heliport, both actors and CAP resources loaded until that scene ends.
extern TaskDesc gActor161500CompleteCompanionRequestTaskDesc;

/// Spawn recipes for this package's soldier B body (0) and carried rifle (1).
///
/// Rooms spawn descriptor 0 with a live Enemy in the second spawn payload.
/// A nonzero first payload creates descriptor 1 as its child, attached to body
/// part 7 and borrowing the body's lighting. Both use priority `TASK_PRIORITY_ACTOR` and suppress
/// automatic model buffers. Keep this overlay's code, models and clips loaded
/// until the tasks are released. The two-entry table has no terminator.
extern TaskDesc gActor161500SoldierBTasks[2];

/// Starts Soldier B's parking conversation and consumes the pending remark.
///
/// Sterilization outcomes take priority over the departed visitor's remark.
/// Requires this actor overlay and the current room's CAP resources to remain
/// loaded until the event script completes.
void actor161500StartSoldierBRemark(void);

/// Starts the next conversation in Soldier B's first heliport dialogue set.
///
/// The saved talk count must be 0..3; it advances to 3 and stays there.
/// Variant 1 selects the alternate four-script set. Keep this actor overlay
/// and the room's CAP resources loaded through event-script completion.
void actor161500StartSoldierBTalkA(void);

/// Starts the next conversation in Soldier B's second heliport dialogue set.
///
/// The saved talk count must be 0..3; it advances to 3 and stays there.
/// Variant 1 selects the alternate four-script set. Keep this actor overlay
/// and the room's CAP resources loaded through event-script completion.
void actor161500StartSoldierBTalkB(void);

/// Starts Soldier B's heliport conversation that opens the room's shop.
///
/// The saved dialogue selector chooses its opening line; both scripts open
/// the shop and resume the conversation after it closes. Keep this actor and
/// the heliport overlay and their resources loaded until the scene finishes.
void actor161500StartSoldierBShopConversation(void);

/// Starts the pending companion-request reminder when the companion is present.
///
/// Placement 3's state selects the reminder line. Requires a live player model
/// and companion model with roots in the same parent frame. Keep this actor
/// overlay and the heliport's CAP resources loaded until the scene finishes.
void actor161500StartCompanionRequestReminder(void);

/// Starts the companion's first request scene or restores its pending scene pose.
///
/// Does nothing without a companion or after the request has been completed.
/// The first scene has a skip script and changes the saved request state to
/// pending; re-entry restores the companion's placement without hiding the HUD.
/// Requires a live player and companion and loaded actor/heliport resources
/// until the selected event script finishes.
void actor161500RestoreCompanionRequestScene(void);

#endif // INCLUDE_ACTORS_ACTOR_161500_H
