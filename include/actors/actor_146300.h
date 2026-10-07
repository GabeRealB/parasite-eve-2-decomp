#ifndef ACTORS_ACTOR_146300_H
#define ACTORS_ACTOR_146300_H

/// Restores the placed actor's animation and placement for the ice-bag handover progress.
///
/// Requires actor_146300 to be loaded and placement 0 in the current room to
/// have its live, initialized actor task. Uses the current stage's object
/// states and the live-save ice-bag collection bit. The final progress value
/// also restores the final placement; progress 4 with a bag follows that same
/// path. Does not consume a bag or advance progress. Requests borrow this
/// overlay's animation and placement records through synchronous dispatch.
void actor146300RestoreHandoverPose(void);

#endif // ACTORS_ACTOR_146300_H
