#ifndef ROOMS_NEO_ARK_OBSERVATORY_H
#define ROOMS_NEO_ARK_OBSERVATORY_H

#include "common.h"

#include "main/task.h"

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

/// Room-local departure descriptor, staged as a whole struct by
/// `func_neo_ark_observatory_8017F6F8` and read by the save-location task
/// `func_neo_ark_observatory_8017F588`.
///
/// `field_4` is the halfword that task forwards as message 0x3EE to the slot-3
/// game pointer; -1 makes it abort the departure and jump to state 2 without
/// sending anything. `field_8` is the sound event it plays through
/// `SndEvt_EnqueueType6` and then polls with `SndVoice_HasActiveId`; 0 skips
/// both and goes straight to the commit. `field_0`..`field_3` are the four
/// bytes the commit copies into `Mc_SaveData`'s 0x5..0x8 save-location block,
/// where 0x0 is the stage id (`field_7`), 0x1 the 1-based area id (`field_6`),
/// 0x2 the direction (`field_8`) and 0x3 the entry (`field_5`).
typedef struct NeoArkObservatoryEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} NeoArkObservatoryEventDesc;
STATIC_ASSERT_SIZEOF(NeoArkObservatoryEventDesc, 0xC);

/// The staged descriptor. `field_4` and `field_8` are also named by address,
/// because the two instructions that read them relocate against their own
/// symbols rather than against the descriptor.
extern NeoArkObservatoryEventDesc D_neo_ark_observatory_80187A30;
extern u16                        D_neo_ark_observatory_80187A34; // D_neo_ark_observatory_80187A30.field_4
extern s32                        D_neo_ark_observatory_80187A38; // D_neo_ark_observatory_80187A30.field_8

/// Departure task: sends the staged 0x3EE message, waits for the 0x3F0 poll to
/// come back clear and for the departure sound to go quiet, then commits the
/// save location above and re-spawns the player task as type 0x11.
void func_neo_ark_observatory_8017F588(Task* arg0);

#endif // ROOMS_NEO_ARK_OBSERVATORY_H
