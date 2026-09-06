#ifndef ROOMS_SHARED_8017E8B4_H
#define ROOMS_SHARED_8017E8B4_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The two panel captions, "Weapon Data" and "PE Data". Each carrying room
/// holds its own copy in its own rodata, so the shared object names them
/// rather than emitting the literals.
extern const char RoomsShared8017e8b4WeaponTitle[];
extern const char RoomsShared8017e8b4PeTitle[];

/// The data panel's list state and the descriptor it spawns its rows from, one
/// pair per carrying room.
extern UiList       RoomsShared8017e8b4List;
extern UiObjectDesc RoomsShared8017e8b4Desc;

/// The save point's weapon/PE data panel: captions itself from the spawn
/// argument, allocates its row scratch and fills the list through whichever of
/// the two row builders the argument selects, then runs the list and closes on
/// a child's result. Nineteen rooms carry this body.
void RoomsShared8017e8b4(Task* task);

#endif // ROOMS_SHARED_8017E8B4_H
