#ifndef WIPSYS_H
#define WIPSYS_H

#include "common.h"

// =============================================================================
// Types — provisional system config / flags (rename when roles are proven)
// =============================================================================

/// WIP: boot/gamemain flag block (Wip_SysFlags). field_4 set on soft-reset paths;
/// field_6 polled/cleared in boot and stream paths. Role not fully proven.
typedef struct _WipSysFlags {
    byte field_0;
    s8   field_1;
    byte unknown_2[2];
    s16  field_4;
    s16  field_6;
    byte unknown_8[0x18];
} WipSysFlags;
STATIC_ASSERT_SIZEOF(WipSysFlags, 0x20);

/// WIP: BSS Wip_SysConfig (0x80). Init by Mc_InitSaveSlotDefaults (four s16s = 100);
/// field_40 filled 0xFF by Mc_InitDualBankBuffers. Likely mix/options block — unproven.
typedef struct _WipSysConfig {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte unknown_C[0xC];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1a;
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  field_1e;
    /* 0x20 */ u8   field_20;
    /* 0x21 */ u8   field_21; // selected item id - 0x7F (`func_800CF448`)
    /* 0x22 */ u8   field_22; // packed into GameActor.field_124 bits 0-7 (`func_801061F0`)
    /* 0x23 */ u8   field_23; // selected item id - 0x5F (`func_800CEC5C`)
    /* 0x24 */ u8   field_24; // cleared/set by func_80104A4C; cleared by func_801053A0; preserved by func_8010C81C
    /* 0x25 */ u8   field_25;
    /* 0x26 */ u8   field_26;
    /* 0x27 */ byte unknown_27[0x19];
    /* 0x40 */ u8   field_40[0x40];
} WipSysConfig;
STATIC_ASSERT_SIZEOF(WipSysConfig, 0x80);

// =============================================================================
// Globals
// =============================================================================

extern WipSysFlags  Wip_SysFlags;
extern WipSysConfig Wip_SysConfig;

#endif // WIPSYS_H
