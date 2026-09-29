#ifndef SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
#define SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingScanState;

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    Task* value;
    u8    retained[4];
} MistParkingStorage532C;
STATIC_ASSERT_SIZEOF(MistParkingStorage532C, 8);

typedef struct {
    /* 0x0 */ u16 timer; // ticks down between companion slots
    /* 0x2 */ s16 slot;  // companion slot 0..4 being walked
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 cmd;   // cap command replayed by state 5
} MistParkingCapState;

extern TaskDesc D_mist_parking_8018D75C[];

extern TaskDesc D_mist_parking_8018FC24[];

extern TaskDesc D_mist_parking_80190824[];

/// Tasks the room keeps a handle on while they run.
extern Task* D_mist_parking_80195318;

extern Task* D_mist_parking_80195320;

extern Task* D_mist_parking_80195324;

extern TmdSource D_mist_parking_80187294;

extern GpAnimSet D_mist_parking_80187594;

extern GpAnimSet D_mist_parking_80187D34;

extern GpAnimSet D_mist_parking_8018821C;

extern GpAnimSet D_mist_parking_801886F8;

extern GpAnimSet D_mist_parking_80188CC0;

extern GpAnimSet D_mist_parking_80189074;

extern GpAnimSet D_mist_parking_80189770;

extern GpAnimSet D_mist_parking_80189C48;

extern GpAnimSet D_mist_parking_8018A340;

extern GpAnimSet D_mist_parking_8018A620;

extern GpAnimSet D_mist_parking_8018A9A4;

extern GpAnimSet D_mist_parking_8018AC64;

extern GpAnimSet D_mist_parking_8018B054;

extern GpAnimSet D_mist_parking_8018B454;

extern GpAnimSet D_mist_parking_8018B790;

extern GpAnimSet D_mist_parking_8018BCA0;

extern GpAnimSet D_mist_parking_8018BFCC;

extern GpAnimSet D_mist_parking_8018C3A0;

extern GpAnimSet D_mist_parking_8018C70C;

extern GpAnimSet D_mist_parking_8018CB34;

extern GpAnimSet D_mist_parking_8018CDB0;

extern GpAnimSet D_mist_parking_8018D220;

extern GpAnimSet D_mist_parking_8018D41C;

extern GpAnimSet D_mist_parking_8018D734;

extern GpCopyArg D_mist_parking_8018D82C;

extern s8 D_mist_parking_8018DA28[28];

extern GpEvsCmd D_mist_parking_8018DF34[155];

extern GpEvsCmd D_mist_parking_8018EDBC[23];

extern GpEvsCmd D_mist_parking_8018EFE4[8];

extern GpEvsCmd D_mist_parking_8018F0A4[10];

extern GpEvsCmd D_mist_parking_8018F194[10];

extern GpGridParams D_mist_parking_8018FCB8;

extern GpAnimSet D_mist_parking_8018FFB8;

extern GpAnimSet D_mist_parking_8019038C;

extern GpAnimSet D_mist_parking_801907FC;

extern GpCopyArg D_mist_parking_80190870;

extern GpAnimArg D_mist_parking_8019088C;

extern GpAnimArg D_mist_parking_80190BC0;

extern GpAnimArg D_mist_parking_80190C10;

extern GpAnimArg D_mist_parking_80190C38;

extern GpAnimArg D_mist_parking_80190C4C;

extern GpAnimArg D_mist_parking_80190C60;

extern GpEvsCmd D_mist_parking_80191154[8];

extern GpEvsCmd D_mist_parking_80191214[10];

extern GpEvsCmd D_mist_parking_80191304[8];

extern GpEvsCmd D_mist_parking_801913C4[8];

extern GpGridParams D_mist_parking_80192204;

extern s32 D_mist_parking_80195310;

extern GpItemMap* D_mist_parking_80195314;

extern s32 D_mist_parking_8019531C;

extern RoomCutsceneRec D_mist_parking_8019533C;

extern MistParkingScanState D_mist_parking_80195328;

extern MistParkingStorage532C D_mist_parking_8019532C;

extern MistParkingCapState D_mist_parking_80195334;

/// Resets the caption state and, for 1 or 2, loads that caption file.
void func_mist_parking_80183708(s32 arg0);

/// Drop the handles of room tasks without killing them; the argument their
/// caller passes is unused.
void func_mist_parking_801837A4(s32 arg0);

void func_mist_parking_8018471C(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_mist_parking_801828F0(Task*);

void func_mist_parking_80183634(s32);

void func_mist_parking_80183688(s32);

void func_mist_parking_801836CC(Task*);

void func_mist_parking_80183780(s32);

void func_mist_parking_80183B40(Task*);

#endif // SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
