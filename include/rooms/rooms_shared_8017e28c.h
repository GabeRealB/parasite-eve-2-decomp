#ifndef ROOMS_SHARED_8017E28C_H
#define ROOMS_SHARED_8017E28C_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The panel's title string and the list the panel drives. Both carrying rooms
/// hold their own pair at their own addresses, named there by the family's
/// symbol maps, so the shared object owns no data.
extern char   RoomsShared8017e28cTitle[];
extern UiList RoomsShared8017e28cMenu;

/// The rooms' BGM-select panel: lays out the room's track list under its title
/// on the first tick, then loops the highlighted track - waiting for the MIDI
/// player to go idle, enqueueing the CD command and re-arming the sound events
/// - until menu or cancel closes the panel. Two rooms carry this body.
void RoomsShared8017e28c(Task* task);

#endif // ROOMS_SHARED_8017E28C_H
