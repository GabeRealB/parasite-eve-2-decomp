#include "common.h"

#include "actors/actor_105400.h"
#include "main/mem.h"
#include "main/sound.h"

/// The spawn's offset pair; `field_8` is the vector the enemy's local
/// position and the second list node are both seeded from.
extern Actor05400Pose D_actor_105400_80133A30;

/// The pair source `GpEnemy::param` points at; its `hpMax` is the HP the
/// context's `field_40` is seeded with.
extern GpPairSrcE D_actor_105400_8013CE30;

/// HP/pose words read straight out of the overlay data: `[0]` is the value
/// stored in `Actor05400Work::field_33C`, which the per-frame handler reads as
/// `Actor05400Work::field_338`.
extern u16 D_actor_105400_8013CE34[];

/// Sound-event base the spawn ORs `(enemy id >> 12) << 8` into.
extern s32 D_actor_105400_8013CE60;

/// One pan/volume row per `gGameSession::at4.loc.view`, played at spawn.
extern Actor05400SndRow D_actor_105400_8013CE64[];

/// Task descriptors the spawn hands `Gp_SpawnEnemyFromTable` (entry 1 is the
/// per-frame dispatcher `func_actor_105400_801337DC`).
extern TaskDesc D_actor_105400_8013CEA0[];

/// Animation bank `func_800B3F84` builds the work block's clip context from.
extern u8 D_actor_105400_8013CEB8[];

/// The message table the task is put on (`Task::msgTable`).
extern u8 D_actor_105400_80133A00[];

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_80131E3C);

INCLUDE_ASM("actors/nonmatchings/actor_105400/actor_105400", func_actor_105400_8013222C);

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_105400/actor_105400", D_actor_105400_80131E30);
