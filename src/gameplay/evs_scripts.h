#ifndef GAMEPLAY_PRIVATE_EVS_SCRIPTS_H
#define GAMEPLAY_PRIVATE_EVS_SCRIPTS_H

#include "gameplay/evs.h"

/// Borrowed scene/audio key retained for event skipping and CAP playback (NULL none).
///
/// Scene selection replaces it; interpreter initialization and END clear it.
/// The key must remain loaded through the interpreter and CAP's last use,
/// including playback debug output. Selecting NULL does not undo the event's
/// separate record that scene streaming must be finished at END.
extern const EvsSceneKey* gEvsSelectedSceneKey;

#endif // GAMEPLAY_PRIVATE_EVS_SCRIPTS_H
