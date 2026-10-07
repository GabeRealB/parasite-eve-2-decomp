#ifndef INCLUDE_ACTORS_ACTOR_160700_H
#define INCLUDE_ACTORS_ACTOR_160700_H

/// Restarts placed actor 0's post-meeting animation when meeting progress is nonzero.
///
/// Requires this actor package and the scene manager to be loaded. A missing
/// placement is harmless. Sends clip 6 with reset playback; the request is
/// consumed synchronously and its package-owned clip storage remains borrowed.
void actor160700RestoreMeetingAnimation(void);

/// Starts the meeting script selected by this actor's saved conversation progress.
///
/// Requires this actor package and the event-script runtime to remain loaded
/// through playback. Progress 0, 1 and 2 starts the first, second and third
/// conversations and advances immediately to 1, 2 and 3. The first two have
/// a common skip script; progress 3 starts a repeat conversation without advancing.
/// Higher values do nothing. Every started script hides and restores the HUD.
/// The first meeting also resets follow-up dialogue and selects story index 12.
void actor160700StartMeetingScript(void);

#endif // INCLUDE_ACTORS_ACTOR_160700_H
