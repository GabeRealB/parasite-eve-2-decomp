#ifndef ROOMS_SHARED_8017D6D0_H
#define ROOMS_SHARED_8017D6D0_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/ui.h"

/// The nine captions the "Play Data" panel prints at the head of each row -
/// play time, saves, area changes, item-use rate, discovery rate, and so on.
/// Every carrying room has its own copy of the strings; the addresses are
/// aliased per overlay in `configs/USA/sym/rooms/<overlay>.txt`.
extern u8 RoomsShared8017d6d0Label0[];
extern u8 RoomsShared8017d6d0Label1[];
extern u8 RoomsShared8017d6d0Label2[];
extern u8 RoomsShared8017d6d0Label3[];
extern u8 RoomsShared8017d6d0Label4[];
extern u8 RoomsShared8017d6d0Label5[];
extern u8 RoomsShared8017d6d0Label6[];
extern u8 RoomsShared8017d6d0Label7[];
extern u8 RoomsShared8017d6d0Label8[];

/// The unit suffix appended after a plain count (rows 1, 2, 3 and 6).
extern u8 RoomsShared8017d6d0Unit[];

/// The nine help strings the row hands to `Ui_SetHolderParam` when the cursor
/// rests on it. They are read through a nine-entry table the compiler builds in
/// this object's own `.rodata`, so the table relocates into whichever room is
/// linking it.
extern u8 RoomsShared8017d6d0Help0[];
extern u8 RoomsShared8017d6d0Help1[];
extern u8 RoomsShared8017d6d0Help2[];
extern u8 RoomsShared8017d6d0Help3[];
extern u8 RoomsShared8017d6d0Help4[];
extern u8 RoomsShared8017d6d0Help5[];
extern u8 RoomsShared8017d6d0Help6[];
extern u8 RoomsShared8017d6d0Help7[];
extern u8 RoomsShared8017d6d0Help8[];

/// Draws one row of the save-point "Play Data" panel. `DialogPrompt::field_8`
/// selects the row: 0 play time, 1 saves, 2 map count, 3 area changes, 4 the
/// item-use ratio, 5 the overall discovery percentage (which also gets a
/// gauge and an extra line of spacing), 6 the continue count, 7 and 8 two
/// running totals. Rows 4 and 5 print a hundredth-of-a-percent integer and
/// then shuffle a decimal point two digits in from the right by hand.
void RoomsShared8017d6d0(DialogPrompt* arg0, UiObject* arg1);

#endif // ROOMS_SHARED_8017D6D0_H
