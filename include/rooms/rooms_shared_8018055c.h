#ifndef INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H
#define INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H

#include "common.h"

#include "main/text.h"

/// One selectable course in a gallery's SELECT menu: the id the controller
/// task remembers in `Task::flags` (so re-picking the same course does not
/// restart the load) and the title drawn beside it ("1. Crazy King", ...).
typedef struct RoomsShared8018055cCourse {
    /* 0x0 */ s32         id;
    /* 0x4 */ const char* name;
} RoomsShared8018055cCourse;
STATIC_ASSERT_SIZEOF(RoomsShared8018055cCourse, 0x8);

/// The ten course lists the SELECT menu can offer, indexed by
/// `Mc_SaveData::gameMode` (the difficulty the save runs at) plus 5 outside a
/// debug attach. The row handler stack-copies the whole table like the other
/// gallery tables, but draws its row through the same 0x28-byte block: the
/// text request overlays the first four list pointers, which is safe because
/// the row is looked up before the request is filled in.
typedef union RoomsShared8018055cMenu {
    /* 0x00 */ RoomsShared8018055cCourse* lists[10];
    /* 0x00 */ TextDrawReq                req;
} RoomsShared8018055cMenu;
STATIC_ASSERT_SIZEOF(RoomsShared8018055cMenu, 0x28);

#endif // INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H
