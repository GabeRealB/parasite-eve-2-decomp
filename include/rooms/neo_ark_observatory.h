#ifndef ROOMS_NEO_ARK_OBSERVATORY_H
#define ROOMS_NEO_ARK_OBSERVATORY_H

#include "common.h"

/// Record inspected by `func_neo_ark_observatory_8017F44C`: `field_0` is the
/// room the marker stands for and `field_5` gates the lookup on 0 - the same
/// shape the map UI's `MapShelterRec` uses, because this body is one of the
/// copies the room overlays carry of the map-marker resolver.
typedef struct MapMarkerRec {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[3];
    /* 0x5 */ u8   field_5;
} MapMarkerRec;

/// Record the resolver writes into: `field_3` receives the marker state the
/// room's GameFlag nibble selects. Called with the same pointer as `MapMarkerRec`,
/// so `field_3` overwrites the record in place.
typedef struct MapMarkerOut {
    /* 0x0 */ byte pad_0[3];
    /* 0x3 */ s8   field_3;
} MapMarkerOut;

#endif // ROOMS_NEO_ARK_OBSERVATORY_H
