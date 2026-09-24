#ifndef ROOMS_NEO_ARK_SHRINE_H
#define ROOMS_NEO_ARK_SHRINE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// A pair of 16-bit coordinates used by the shrine's sliding-tile puzzle: the
/// screen position of a tile's quad, or the texture origin it samples from.
/// The puzzle tables index this by tile number or by board position.
typedef struct {
    u16 x;
    u16 y;
} NeoArkShrineSlot;

/// Scratch state of the shrine's cap script, stored at `Task::work`
/// (`memCalloc(0x10)` in `func_neo_ark_shrine_8017ECC4`).
typedef struct {
    /* 0x00 */ u8  pad_0[8];
    /* 0x08 */ u16 timer; ///< frames the current script step has run
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  field_F;
} NeoArkShrineScript;

/// State table of the shrine's cap script task, indexed by `Task::state`.
extern const TaskFuncTable16 D_neo_ark_shrine_8017D5D0;

/// State table of the shrine's first falling prop, indexed by `Task::state`.
extern const TaskFuncTable4 D_neo_ark_shrine_8017D610;

/// State table of the shrine's second falling prop, indexed by `Task::state`.
extern const TaskFuncTable3 D_neo_ark_shrine_8017D620;

/// Hotspot table of the shrine's cap script, terminated by an `id` of -1.
extern RoomHotspot D_neo_ark_shrine_80182430[];

extern TaskDesc D_neo_ark_shrine_80182508;

/// Set by one of the cap script's two pad-lerp steps and cleared by the other;
/// while it is set, the puzzle's step check reports kind 4.
extern s16 D_neo_ark_shrine_80186868;

/// Set by the cap script step that ends the prompt task. The puzzle's step
/// check consumes it to switch the room's layout, and the first falling prop
/// waits for it to clear.
extern s16 D_neo_ark_shrine_8018686A;

/// Current order index of each of the 16 slots. Read back through a `u16`
/// pointer where the puzzle swaps two of them, which is why those accesses are
/// unsigned while the rest are `s16`.
extern s16 D_neo_ark_shrine_8018686C[16];

/// Drawn position of each puzzle tile, eased towards its target every frame.
extern NeoArkShrineSlot D_neo_ark_shrine_8018688C[16];

void func_neo_ark_shrine_8017DF7C(void);

/// Per-frame helper of the cap script, declared without a parameter list
/// because some callers pass it their `task`: the extra argument setup is what
/// their generated code needs, and the helper ignores it.
void func_neo_ark_shrine_8017EAC0();

s32  func_neo_ark_shrine_8017EC10(RoomHotspot* table, s16 x, s16 y);
void func_neo_ark_shrine_8017ECC4(Task* task);
void func_neo_ark_shrine_8017EDAC(Task* task);
void func_neo_ark_shrine_8017EDE0(Task* task);
void func_neo_ark_shrine_8017EE44(Task* task);
void func_neo_ark_shrine_8017EED4(Task* task);
void func_neo_ark_shrine_8017EF68(Task* task);
void func_neo_ark_shrine_8017EFE4(Task* task);
void func_neo_ark_shrine_8017F094(Task* task);
void func_neo_ark_shrine_8017F0F0(Task* task);
void func_neo_ark_shrine_8017F178(Task* task);
void func_neo_ark_shrine_8017F21C(Task* task);
void func_neo_ark_shrine_8017F274(Task* task);
void func_neo_ark_shrine_8017F320(Task* task);
void func_neo_ark_shrine_8017F398(Task* task);
void func_neo_ark_shrine_8017F448(void);
void func_neo_ark_shrine_8017F4C8(Task* task);
void func_neo_ark_shrine_8017F578(Task* task);
void func_neo_ark_shrine_8017F640(Task* task);
void func_neo_ark_shrine_8017F688(Task* task);
void func_neo_ark_shrine_8017F738(Task* task);

#endif // ROOMS_NEO_ARK_SHRINE_H
