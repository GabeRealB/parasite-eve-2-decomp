#ifndef ACTORS_SHARED_80136288_H
#define ACTORS_SHARED_80136288_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::work`, which is not a `TaskIdMap` here). The three `GpObj`s are the
/// display nodes `ActorsShared80138570` unlinks on its own exit path.
///
/// `field_36C` is the mode this body dispatches on - the sibling state machine
/// `ActorsShared80137e18` writes is the `field_36A`/`field_36E` pair, and this
/// one counts its own reaction out in `field_36E`. `field_370`/`field_372`/
/// `field_374` are the same (helper id, id the slots last saw, frames spent on
/// it) triple `Actor107000Work` keeps at 0x370 and `Actor207200Work` at 0x48C,
/// and `field_394` is the same already-reacted flag `ActorShared8014d378Work`
/// reads at 0x394.
typedef struct ActorShared80136288Work {
    /* 0x000 */ byte  pad_0[0x1DC];
    /* 0x1DC */ GpObj field_1DC;
    /* 0x1FC */ byte  pad_1FC[0x30];
    /* 0x22C */ GpObj field_22C;
    /* 0x24C */ byte  pad_24C[0x60];
    /* 0x2AC */ GpObj field_2AC;
    /* 0x2CC */ byte  pad_2CC[0xA0];
    /* 0x36C */ s16   field_36C; // mode this handler dispatches on
    /* 0x36E */ u16   field_36E; // frames the reaction has run; 0x3D moves to mode 2
    /* 0x370 */ s16   field_370; // helper id the six slots are rebound to
    /* 0x372 */ u16   field_372; // id the six helper slots last saw
    /* 0x374 */ u16   field_374; // frames spent on the current helper id
    /* 0x376 */ byte  pad_376[0x1E];
    /* 0x394 */ u16   field_394; // non-zero: this frame has spent its reaction
} ActorShared80136288Work;

/// Which of the alternate modes the current frame runs, keyed on
/// `gGameSession`'s scenario id.
extern u8 D_801153F4;

/// Per-frame mode handler of the specimen's first state machine, shared by
/// `actor_107000` and `actor_207000`. Mode 1 skips to the tail, mode 2 ORs the
/// hidden pose onto the model part's flag word and returns. The body dispatches
/// the work's `field_36C`: mode 0 cues the death sound, puts the part in its
/// 2-pose, clears the enemy's `field_54` slot, unlinks the enemy node and the
/// work's three display nodes, releases the state slot and steps to mode 1 with
/// helper id 0xC; mode 1 counts `field_36E` out to 0x3D before dropping to 2,
/// and hides the part once `field_394` says this frame has already spent its
/// reaction; mode 2 cues the impact sound, hides the part, re-parents the
/// model's second coordinate part to the first and puts the task in state 3.
/// The tail then rebinds the work's six helper slots to `field_370`: a changed
/// id restarts `field_374` and points every slot at it at weight 8, otherwise
/// the id is advanced and `field_374` counts the frames spent on it.
///
/// The tail re-reads the work into a local of its own - folding the two reads
/// into one would make that one pseudo span both loops below.
void ActorsShared80136288(GpEnemy* arg0, Task* arg1);

#endif // ACTORS_SHARED_80136288_H
