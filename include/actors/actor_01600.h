#ifndef ACTORS_ACTOR_01600_H
#define ACTORS_ACTOR_01600_H

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

typedef struct Actor01600Range {
    /* 0x0 */ s32 low;
    /* 0x4 */ s32 high;
} Actor01600Range;
STATIC_ASSERT_SIZEOF(Actor01600Range, 0x8);

/// Actor-side view of a collision key, including its body class and slot bit.
typedef union Actor01600ContactId {
    s32 id;
    struct {
        u8  byte0;
        u8  byte1;
        u16 kind;
    } parts;
} Actor01600ContactId;

typedef struct Actor01600Contact {
    u16                 flags;
    s16                 depth;
    Actor01600ContactId hit;
    SVECTOR             point;
    SVECTOR             normal;
} Actor01600Contact;
STATIC_ASSERT_SIZEOF(Actor01600Contact, sizeof(GpRec18));

/// The collider's shared contact table and the actor's packed-key view.
typedef struct Actor01600Contacts {
    GpObj obj;
    union {
        GpRec18           recs[8];
        Actor01600Contact hits[8];
    } entries;
} Actor01600Contacts;
STATIC_ASSERT_SIZEOF(Actor01600Contacts, 0xE0);

typedef struct Actor01600Work {
    /* 0x000 */ GpAnimCtx          anim;
    /* 0x014 */ GpAnimSlot         slots[9];
    /* 0x17C */ byte               pad_17C[0x90];
    /* 0x20C */ MATRIX             field_20C;
    /* 0x22C */ MATRIX             field_22C;
    /* 0x24C */ GpCoord            field_24C;
    /* 0x29C */ byte               field_29C[8];
    /* 0x2A4 */ GpCoord*           field_2A4;
    /* 0x2A8 */ s8*                field_2A8;
    /* 0x2AC */ s16                field_2AC;
    /* 0x2AE */ s16                field_2AE;
    /* 0x2B0 */ s16                field_2B0;
    /* 0x2B2 */ s16                field_2B2;
    /* 0x2B4 */ s32                field_2B4;
    /* 0x2B8 */ s16                field_2B8;
    /* 0x2BA */ u16                field_2BA;
    /* 0x2BC */ byte               pad_2BC[4];
    /* 0x2C0 */ s16                field_2C0;
    /* 0x2C2 */ byte               pad_2C2[0xA];
    /* 0x2CC */ s16                field_2CC;
    /* 0x2CE */ s16                field_2CE;
    /* 0x2D0 */ GpRec18*           field_2D0;
    /* 0x2D4 */ GpRec18            field_2D4;
    /* 0x2EC */ Actor01600Contacts collision;
    /* 0x3CC */ byte               field_3CC[8];
    /* 0x3D4 */ GpCoord*           field_3D4;
    /* 0x3D8 */ GpRec18*           field_3D8;
    /* 0x3DC */ s16                field_3DC;
    /* 0x3DE */ s16                field_3DE;
    /* 0x3E0 */ s16                field_3E0;
    /* 0x3E2 */ s16                field_3E2;
    /* 0x3E4 */ s32                field_3E4;
    /* 0x3E8 */ s16                field_3E8;
    /* 0x3EA */ u16                field_3EA;
    /* 0x3EC */ GpRec18            contact_3EC;
    /* 0x404 */ GpEffArg           hitEffect;
    /* 0x40C */ byte               field_40C[8];
    /* 0x414 */ GpCoord*           field_414;
    /* 0x418 */ s8*                field_418;
    /* 0x41C */ s16                field_41C;
    /* 0x41E */ s16                field_41E;
    /* 0x420 */ s16                field_420;
    /* 0x422 */ s16                field_422;
    /* 0x424 */ s32                field_424;
    /* 0x428 */ s16                field_428;
    /* 0x42A */ u16                field_42A;
    /* 0x42C */ s16                field_42C;
    /* 0x42E */ byte               pad_42E[2];
    /* 0x430 */ s16                field_430;
    /* 0x432 */ byte               pad_432[0xA];
    /* 0x43C */ s16                field_43C;
    /* 0x43E */ s16                field_43E;
    /* 0x440 */ s8*                field_440;
    /* 0x444 */ GpRec18            field_444;
    /* 0x45C */ Actor01600Range    ranges[8];
    /* 0x49C */ MATRIX             field_49C;
    /* 0x4BC */ s32                field_4BC;
    /* 0x4C0 */ s32                field_4C0;
    /* 0x4C4 */ s32                field_4C4;
    /* 0x4C8 */ byte               pad_4C8[4];
    /* 0x4CC */ s16                field_4CC;
    /* 0x4CE */ byte               pad_4CE[6];
    /* 0x4D4 */ Task*              field_4D4;
    /* 0x4D8 */ s16                field_4D8;
    /* 0x4DA */ s16                field_4DA;
    /* 0x4DC */ s16                field_4DC;
    /* 0x4DE */ byte               pad_4DE[2];
    /* 0x4E0 */ s32                field_4E0;
    /* 0x4E4 */ s32                field_4E4;
    /* 0x4E8 */ s16                field_4E8;
    /* 0x4EA */ s16                field_4EA;
    /* 0x4EC */ s16                field_4EC;
    /* 0x4EE */ s16                field_4EE;
    /* 0x4F0 */ s16                field_4F0;
    /* 0x4F2 */ s16                field_4F2;
    /* 0x4F4 */ u16                field_4F4;
    /* 0x4F6 */ s16                field_4F6;
    /* 0x4F8 */ s16                field_4F8;
    /* 0x4FA */ s16                field_4FA;
    /* 0x4FC */ s16                field_4FC;
    /* 0x4FE */ s16                field_4FE;
    /* 0x500 */ s16                field_500;
    /* 0x502 */ s16                field_502;
    /* 0x504 */ s16                field_504;
    /* 0x506 */ s16                field_506;
    /* 0x508 */ s16                field_508;
    /* 0x50A */ s16                field_50A;
    /* 0x50C */ s16                field_50C;
    /* 0x50E */ s16                field_50E;
    /* 0x510 */ s16                field_510;
    /* 0x512 */ s16                field_512;
    /* 0x514 */ s16                field_514;
    /* 0x516 */ s16                field_516;
    /* 0x518 */ s16                field_518;
    /* 0x51A */ s16                field_51A;
    /* 0x51C */ s16                field_51C;
    /* 0x51E */ s16                field_51E;
    /* 0x520 */ s16                field_520;
    /* 0x522 */ s16                field_522;
    /* 0x524 */ s16                field_524;
    /* 0x526 */ s16                field_526;
    /* 0x528 */ s16                field_528;
    /* 0x52A */ s16                field_52A;
    /* 0x52C */ s16                field_52C;
    /* 0x52E */ s16                field_52E;
    /* 0x530 */ s16                field_530;
    /* 0x532 */ s16                field_532;
    /* 0x534 */ s16                field_534;
    /* 0x536 */ s16                field_536;
    /* 0x538 */ s16                field_538;
    /* 0x53A */ s16                field_53A;
    /* 0x53C */ s16                field_53C;
    /* 0x53E */ s16                field_53E;
    /* 0x540 */ s16                field_540;
    /* 0x542 */ u16                field_542;
    /* 0x544 */ s16                field_544;
    /// Second animation id the `variant == 2` and `variant == 4` paths of
    /// `Actor01600_Fn05F80` run their countdown against: it is stored into
    /// `field_506` and steps 7 -> 9.
    /* 0x546 */ s16 field_546;
    /* 0x548 */ u16 field_548;
    /// Copy of the spawn variant `Actor01600_Fn05F80` takes its `case 0x1A`
    /// path for.
    /* 0x54A */ s16  field_54A;
    /* 0x54C */ u16  field_54C;
    /* 0x54E */ s16  field_54E;
    /* 0x550 */ s16  field_550;
    /* 0x552 */ byte pad_552[2];
    /* 0x554 */ s16  field_554;
    /* 0x556 */ s16  field_556;
} Actor01600Work;
STATIC_ASSERT_SIZEOF(Actor01600Work, 0x558);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, collision.entries) == 0x30C, actor01600_contact_offset);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, contact_3EC) == 0x3EC, actor01600_single_contact_offset);
STATIC_ASSERT(OFFSET_OF(Actor01600Work, hitEffect) == 0x404, actor01600_effect_offset);

#endif
