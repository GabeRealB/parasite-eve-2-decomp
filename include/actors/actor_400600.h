#ifndef ACTOR_400600_H
#define ACTOR_400600_H

#include "common.h"
#include "main/task.h"

/// Three packed halfwords filled by `func_actor_400600_80139F4C`: the actor's
/// view-space X/Y (the translation of the `Gp_WorldToLocal` result) and the low
/// half of the root coordinate's world Z. `func_actor_400600_801361AC` passes
/// `Actor400600Work::field_88` as the destination, so the slot lives inside the
/// work block.
typedef struct Actor400600ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor400600ViewPos;
STATIC_ASSERT_SIZEOF(Actor400600ViewPos, 0x6);

/// The word at `Actor400600Work::field_710`. Its low half is a frame countdown
/// (`func_actor_400600_80138AB8` ticks it down, `func_actor_400600_8013C074`
/// seeds it from the LCG); its high half is a flag halfword.
/// `func_actor_400600_8013892C` tests bit 0 of `flags` on its own and then the
/// whole word against `0x01020000`, so both views are modelled explicitly.
typedef union Actor400600Timer {
    /* 0x0 */ s32 word;
    struct {
        /* 0x0 */ s16 timer;
        /* 0x2 */ u16 flags;
    } h;
} Actor400600Timer;
STATIC_ASSERT_SIZEOF(Actor400600Timer, 0x4);

/// Per-actor state block for the `actor_400600` overlay.
///
/// `func_actor_400600_80133434` allocates it with `Mem_Calloc(0x770)` and
/// stores it in the `Task::idMap` slot (0x1C): an enemy actor reuses that
/// pointer field for its own work block, so it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor400600Work*)task->idMap`.
typedef struct Actor400600Work {
    /* 0x000 */ byte               pad_0[0x88];
    /* 0x088 */ Actor400600ViewPos field_88;
    /* 0x08E */ byte               pad_8E[0x4];
    /* 0x092 */ u16                field_92;  // seeds field_73E on state entry
    /* 0x094 */ byte               pad_94[0x58E];
    /* 0x622 */ u16                field_622; // flags; bit 0x4000 cleared
    /* 0x624 */ byte               pad_624[0xE0];
    /* 0x704 */ Task*              field_704; // child task, killed on death
    /* 0x708 */ Task*              field_708; // child task, killed on death
    /* 0x70C */ byte               pad_70C[0x4];
    /* 0x710 */ Actor400600Timer   field_710;
    /* 0x714 */ byte               pad_714[0x4];
    /* 0x718 */ u16                field_718; // per-state frame counter
    /* 0x71A */ byte               pad_71A[0x2];
    /* 0x71C */ u16                field_71C; // state index
    /* 0x71E */ u16                field_71E; // sub-state index
    /* 0x720 */ s16                field_720;
    /* 0x722 */ s16                field_722; // velocity step (can go negative)
    /* 0x724 */ s16                field_724; // accumulated step
    /* 0x726 */ s16                field_726;
    /* 0x728 */ byte               pad_728[0xA];
    /* 0x732 */ s16                field_732; // countdown seeded by func_actor_400600_80138AF0
    /* 0x734 */ byte               pad_734[0x6];
    /* 0x73A */ s16                field_73A; // fade level, lerped toward 0xFF
    /* 0x73C */ byte               pad_73C[0x2];
    /* 0x73E */ u16                field_73E;
    /* 0x740 */ byte               pad_740[0x2];
    /* 0x742 */ s16                field_742; // animation request kind
    /* 0x744 */ byte               pad_744[0x2];
    /* 0x746 */ s16                field_746; // animation id
    /* 0x748 */ s16                field_748; // sound step index (func_actor_400600_801361AC)
    /* 0x74A */ byte               pad_74A[0x19];
    /* 0x763 */ s8                 field_763;
    /* 0x764 */ byte               pad_764[0x1];
    /* 0x765 */ s8                 field_765;
    /* 0x766 */ s8                 field_766;
    /* 0x767 */ s8                 field_767;
    /* 0x768 */ byte               pad_768[0x8];
} Actor400600Work;
STATIC_ASSERT_SIZEOF(Actor400600Work, 0x770);

#endif
