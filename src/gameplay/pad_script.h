#ifndef GAMEPLAY_PRIVATE_PAD_SCRIPT_H
#define GAMEPLAY_PRIVATE_PAD_SCRIPT_H

// Gameplay-private pad-script entry points.

/// Clears the vibration interpreter and both motor tasks' halt gates.
///
/// Does not restore cleared requests, session flags or tasks already torn down.
/// Gameplay's player-input update calls this once per frame with a player task.
void padScriptClearHalt(void);

#endif // GAMEPLAY_PRIVATE_PAD_SCRIPT_H
