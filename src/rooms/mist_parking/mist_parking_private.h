#ifndef MIST_PARKING_PRIVATE_H
#define MIST_PARKING_PRIVATE_H

#include "common.h"

#include "main/task_types.h"

typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingScanState;

extern MistParkingScanState D_mist_parking_80195328;

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    Task * value;
    u8 retained[4];
} MistParkingStorage532C;
STATIC_ASSERT_SIZEOF(MistParkingStorage532C, 8);

extern MistParkingStorage532C D_mist_parking_8019532C;

// Callbacks referenced by the overlay's shared data tables.
void func_mist_parking_801828F0(Task *);
void func_mist_parking_80183634(s32);
void func_mist_parking_80183688(s32);
void func_mist_parking_801836CC(Task *);
void func_mist_parking_80183780(s32);
void func_mist_parking_80183B40(Task *);

typedef struct {
    /* 0x0 */ u16 timer; // ticks down between companion slots
    /* 0x2 */ s16 slot;  // companion slot 0..4 being walked
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 cmd;   // cap command replayed by state 5
} MistParkingCapState;

extern MistParkingCapState D_mist_parking_80195334;

#endif // MIST_PARKING_PRIVATE_H
