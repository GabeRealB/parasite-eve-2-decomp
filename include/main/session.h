#ifndef SESSION_H
#define SESSION_H

#include "common.h"

#include <psyq/libgte.h>

// =============================================================================
// Types — game session / actor objects
// =============================================================================

/// 4-byte block assigned via unaligned lwl/lwr (see Snd_ApplyVolumeTable).
typedef struct _GBytes4 {
    u8 data[4];
} GBytes4;

/// 6-byte block assigned via unaligned lwl/lwr + lb/sb (see TaskIdMap_RemapIndex).
typedef struct _GBytes6 {
    u8 data[6];
} GBytes6;

/// 8 raw bytes of a record, for the places where the block is copied as
/// bytes and no field is named. `GameLoc.raw` is the arm that gives the
/// location cell its width, and the byte view its callers hand on.
typedef struct {
    u8 data[8];
} GBytes8;

/// 18-byte MATRIX rotation (3x3 s16). Assigned via unaligned lwl/lwr + lh/sh
/// (see Gp_ApplyView). The trailing s16 (not u8[2]) keeps the last two bytes
/// a halfword; a pure u8[18] emits lb/sb instead.
typedef struct _GBytes18 {
    u8  data[0x10];
    s16 field_10;
} GBytes18;

struct Task;

/// 6-byte key of a place in the world.
///
/// `stage` / `area` select `Gp_AreaTables`; `place` indexes the nested record
/// (written from the area object's id). `view`, `room` and `warp` select the
/// per-room view, the room within the area, and the per-area warp. The same
/// key is `GameSession.at4.loc` and `Mc_SaveData.at4.loc`, and is passed into
/// area, view, room and warp lookups.
typedef struct GpAreaKey {
    u8 view;  // 1-based view slot; innermost index of the per-room view table
    u8 room;  // 1-based room index within the area
    u8 area;  // 1-based area / CDF folder within the stage
    u8 stage; // 1-based stage; indexes per-stage tables
    u8 warp;  // 1-based warp slot; indexes the per-area warp table
    u8 place; // nested place index, synced from the area object's id
} GpAreaKey;
STATIC_ASSERT_SIZEOF(GpAreaKey, 0x6);

/// 8-byte location cell: the 6-byte place key followed by two bytes a
/// whole-cell copy also moves. `loc` is the key the lookups index by; `raw` is
/// the whole cell, used by the copies between the session and the save and by
/// the callers that hand the cell on as bytes.
typedef union {
    GpAreaKey loc;
    GBytes8   raw;
} GameLoc;
STATIC_ASSERT_SIZEOF(GameLoc, 8);

/// Live play-state object shared by main and every overlay.
///
/// One BSS instance is pointed to by `gGameSession`. It holds the current
/// place in the world, the tasks the session keeps by slot, remapped pad
/// buttons, and the flags that cutscenes, view loads and death/restart share.
/// New game, load and reset zero the whole object, which pins the size at 0x13C.
typedef struct {
    s8           deathVariant;   // (0 none, 1/2 which death cutscene file); nonzero blocks resume and menu
    s8           eventState;     // 0 idle; nonzero blocks player-dir handling and room scripts
    u8           uiOpen;         // 1 while a UI overlay is up; enables d-pad auto-repeat
    byte         unknown_3;
    GameLoc      at4;            // current place in the world
    struct Task* ptrSlots[16];   // tasks the session keeps by slot
    u8           applySavePlace; // 1: next area load writes the save's place id into the area object
    u8           viewReady;      // (0 loading/transitioning, 1 current view finished loading)
    u16          field_4E;
    byte         unknown_50[2];
    s16          viewDirty;      // nonzero: respawn the view from the save
    byte         unknown_54[4];
    u16          pad;            // remapped buttons this frame
    u16          padPrev;        // remapped buttons last frame
    u16          padTrig;        // remapped buttons newly pressed this frame
    u8           field_5E;
    u8           evtSkipped;     // 1 after a forced script skip; nonzero ends overlay and timed waits early
    byte         unknown_60[4];
    u8           freezeRoomObjs; // nonzero: skip room-object state dispatch
    u8           field_65;
    u8           cutsceneHold;   // 1 during scripted sequences: alternate item menu, player hold
    byte         areaSetupDone;  // 0 first area setup skips warp-arrival SFX/flag and latches to 1
    u8           hideHud;        // 1: suppress item prompt, HUD, and target cursor
    u8           flowFlags;      // bit0 skip ending bank-load; bit1 skip area-enter bank-load; bit2 ending spawn 3 vs 2; bit3 area-enter spawn 3 vs 1; bit6 hide weapon with bit7; bit7 PE re-equip
    byte         unknown_6A[0xA];
    u16          sprtVariant;    // 1-based sprite-table / CdCmd param2[0] variant; USA forces 1
    s16          roomObjsDirty;  // nonzero: relink room objects on the next room-obj tick
    s16          loadedStage;    // last stage whose CD was enqueued
    byte         unknown_7A[2];
    s16          field_7C;
    s16          field_7E;
    s16          field_80;
    byte         unknown_82[0x9A];
    s16          loadedWeaponFamily;  // last-loaded player weapon-anim family; -1 forces a CD refresh
    s16          loadedConfigSet;     // last-loaded player config-set; paired with loadedWeaponFamily
    s16          sceneClock;          // frame countdown for timed scene scripts
    s16          waterY;              // water surface world Y
    u8           companionType;       // (0 none, 1/2/3)
    u8           companionVariant;    // addend within the companion ally-id family
    u8           field_126;
    u8           suppressDeathChecks; // nonzero: skip player/companion-down handling
    u8           restartMode;         // (0, 1 companion-1 down, 3 special, 4 companion-3 down, 0xFF ending load)
    u8           loadedSndId;         // last sound-file id already queued; skip re-enqueue when unchanged
    s16          bossPartsHpSum;      // sum of living boss-part HP; scales later enemy spawn HP
    u8           field_12C;
    s8           areaBgmCountdown;    // frames before area BGM on the death path; 0x7F holds without counting
    u8           field_12E;
    u8           deathRestartDelay;   // frames the play-clock waits before BGM/restart after death
    byte         spawnPhase[2];       // (0 idle, 1 armed, 2 done) per spawn-controller slot
    u8           field_132;
    byte         field_133;
    byte         eventRoomIndex; // 0-based room echoed into event replies
    u8           field_135;
    u8           enemyCullZone;  // 1..16 index into the enemy axis-limit table; 0 disables
    byte         skipEventIntro; // nonzero: skip intro spawns
    byte         unknown_138;
    u8           hudShakeY;      // signed HUD vertical shake amplitude (pixels x 3)
    u8           dirActionBusy;  // 1 while a direction/cap action is in flight; blocks HUD
    u8           padScriptFlags; // bit0 hold, bit1 lerp, bit7 run pad scripts during battle freeze
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);
STATIC_ASSERT(OFFSET_OF(GameSession, at4) == 4, GameSession_at4);
STATIC_ASSERT(OFFSET_OF(GameSession, at4.loc) == 4, GameSession_at4_loc);
STATIC_ASSERT(OFFSET_OF(GameSession, ptrSlots) == 0xC, GameSession_ptrSlots);
struct _GpLinkNode;
struct _GpActorD4;
struct _GpAnimRec;

/// 0x18 record wiped by `Gp_InitRec18Table`. That helper zeros `count` entries
/// and writes 2 to the last element's `field_0`. `field_0` bit 0x1 marks an
/// occupied slot; bit 0x2 marks the last element. `Gp_FindRec18` returns the
/// 1-based index of the last occupied slot whose `field_4` equals `arg1`,
/// or 1 as soon as any occupied slot is seen when `arg1` is 0.
/// `Gp_CountRec18Hi` counts occupied slots whose `field_4` high 16 bits match
/// `arg1`. `Gp_ClearRec18Occupied` walks until bit 0x2, and for each occupied slot
/// keeps only that last-element bit and zeros the payload halfwords / word
/// (leaving 0xE and 0x16 untouched). Embedded as `GameActor.field_17C[18]`;
/// `func_801041B4` tests `field_4` bits 0x100100.
typedef struct _GpRec18 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ s16  field_2;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ byte pad_E[2];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
} GpRec18;
STATIC_ASSERT_SIZEOF(GpRec18, 0x18);

/// 0x28-byte record in `GameActor.field_438`, the actor's own `GpAnimSlot`
/// array (`gameplay/1BC.h`); `GameActor.field_424` is the `GpAnimCtx` whose
/// `field_C` points here. Count is `GameActor.field_938` (init 0x13).
/// Restated main-side so `session.h` does not depend on a gameplay header:
/// `field_4` is `GpAnimSlot::field_4`, `field_9` the clamped 1..0x7F
/// slid-actor byte written by `func_801058BC`, and `field_10` the flag
/// halfword (bits 0/1: `Gp_TickActorAnimState` case 8; bit 0x100:
/// `func_8010583C`; bits 0x102: `func_80105894`).
typedef struct _GameActorSlot {
    /* 0x00 */ byte pad_0[4];
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[3];
    /* 0x09 */ u8   field_9;
    /* 0x0A */ byte pad_A[6];
    /* 0x10 */ u16  field_10;
    /* 0x12 */ byte pad_12[0x16];
} GameActorSlot;
STATIC_ASSERT_SIZEOF(GameActorSlot, 0x28);

/// Large object pointed to by Task::work for the slot-3 game object
/// (Game_GetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ s32                 field_0;  // per-frame X velocity (Gp_PlayerMode2State3)
    /* 0x004 */ s32                 field_4;  // per-frame Y velocity
    /* 0x008 */ s32                 field_8;  // per-frame Z velocity
    /* 0x00C */ byte                pad_C[4];
    /* 0x010 */ s32                 field_10; // copy of GsCOORDINATE2.coord.t[0]
    /* 0x014 */ s32                 field_14; // copy of GsCOORDINATE2.coord.t[1]
    /* 0x018 */ s32                 field_18; // copy of GsCOORDINATE2.coord.t[2]
    /* 0x01C */ byte                pad_1C[4];
    /* 0x020 */ s32                 field_20; // copied from Gp_SetActorDest arg2
    /* 0x024 */ s32                 field_24;
    /* 0x028 */ s32                 field_28;
    /* 0x02C */ byte                pad_2C[4];
    /* 0x030 */ VECTOR              field_30; // push-back dir (func_80109BB4); low halves go to a scratch SVECTOR
    /* 0x040 */ s32                 field_40;
    /* 0x044 */ s32                 field_44;
    /* 0x048 */ s32                 field_48;
    /* 0x04C */ byte                pad_4C[4];
    /* 0x050 */ s16                 field_50; // SVECTOR.vx; func_80104D68 / RotMatrix
    /* 0x052 */ s16                 field_52; // facing angle (lh); func_8010BCF4 / func_80103E7C
    /* 0x054 */ s16                 field_54; // SVECTOR.vz; func_80104D68 / RotMatrix
    /* 0x056 */ byte                pad_56[2];
    /* 0x058 */ s16                 field_58;
    /* 0x05A */ byte                pad_5A[2];
    /* 0x05C */ s16                 field_5C; // pitch; Gp_AimPitchToLockAlt
    /* 0x05E */ byte                pad_5E[2];
    /* 0x060 */ s16                 field_60;
    /* 0x062 */ byte                pad_62[2];
    /* 0x064 */ s16                 field_64; // pitch; Gp_AimPitchToLockAlt
    /* 0x066 */ byte                pad_66[2];
    /* 0x068 */ s16                 field_68;
    /* 0x06A */ s16                 field_6A;       // aim/look yaw offset; func_8010BE5C
    /* 0x06C */ byte                pad_6C[4];
    /* 0x070 */ s16                 field_70;       // pitch-like angle; Gp_AimPitchRec
    /* 0x072 */ byte                pad_72[6];
    /* 0x078 */ s16                 field_78;       // pitch; Gp_AimPitchDirect
    /* 0x07A */ byte                pad_7A[6];
    /* 0x080 */ s16                 field_80;       // copied from func_80104F5C arg2
    /* 0x082 */ s16                 field_82;       // target facing angle; func_80104E00 / Gp_PlayerMode2State2
    /* 0x084 */ byte                pad_84[4];
    /* 0x088 */ byte                field_88[8];    // address taken as GpObj.field_C
    /* 0x090 */ s32                 field_90;
    /* 0x094 */ byte                field_94[8];    // address taken as GpObj.field_C
    /* 0x09C */ s32                 field_9C;
    /* 0x0A0 */ byte                field_A0[8];    // address taken as GpObj.field_C
    /* 0x0A8 */ s32                 field_A8;
    /* 0x0AC */ byte                field_AC[0x20]; // 0x20-byte list node (Gp_UnlinkObj)
    /* 0x0CC */ byte                field_CC[0x20];
    /* 0x0EC */ byte                field_EC[0x20];
    /* 0x10C */ byte                field_10C[0x18]; // 0x20-byte list node; field_124 is +0x18
    /* 0x124 */ u32                 field_124;
    /* 0x128 */ byte                pad_128[2];
    /* 0x12A */ u16                 field_12A;
    /* 0x12C */ byte                field_12C[0x20];
    /* 0x14C */ byte                field_14C[0x18]; // GpActorD4Rec; Gp_AttachActorObj
    /* 0x164 */ byte                pad_164[0x18];
    /* 0x17C */ GpRec18             field_17C[18];   // Gp_ClearRec18Occupied / func_801041B4
    /* 0x32C */ GpRec18             field_32C[6];    // Gp_AttachActorObj / Gp_InitRec18Table
    /* 0x3BC */ byte                pad_3BC[0x18];
    /* 0x3D4 */ byte                field_3D4[0x50]; // GsCOORDINATE2; Gp_AttachActorObj
    /* 0x424 */ byte                field_424[0x14]; // GpAnimCtx overlay; Gp_AnimTickIndex
    /* 0x438 */ GameActorSlot       field_438[19];   // GpAnimSlot array; func_80105B0C
    /* 0x730 */ byte                pad_730[0x10];
    /* 0x740 */ byte                pad_740[0x68];
    /* 0x7A8 */ byte                field_7A8; // addr taken as func_800B3F84 arg3
    /* 0x7A9 */ byte                pad_7A9[0x163];
    /* 0x90C */ struct _GpLinkNode* field_90C;
    /* 0x910 */ struct _GpActorD4*  field_910;
    /* 0x914 */ struct Task*        field_914;
    /* 0x918 */ struct Task*        field_918;
    /* 0x91C */ struct Task*        field_91C;
    /* 0x920 */ struct Task*        field_920;
    /* 0x924 */ struct Task*        field_924;
    /* 0x928 */ void*               field_928; // Gp_PlayerAnimBlkTbl[field_93A]; func_800B3F84 arg1
    /* 0x92C */ struct _GpAnimRec*  field_92C; // last Gp_AnimGetRec result (Gp_PlayerNormalState5)
    /* 0x930 */ s32                 field_930; // sw from Gp_MsgPlayerDirFacing; addr taken by func_801011D0
    /* 0x934 */ s32                 field_934;
    /* 0x938 */ s16                 field_938; // GameActorSlot count (init 0x13)
    /* 0x93A */ u16                 field_93A; // Gp_WeaponIdBase[field_22-1] + field_21
    /* 0x93C */ u16                 field_93C;
    /* 0x93E */ s16                 field_93E;
    /* 0x940 */ s16                 field_940;
    /* 0x942 */ s16                 field_942;
    /* 0x944 */ s16                 field_944;
    /* 0x946 */ s16                 field_946;
    /* 0x948 */ s16                 field_948;
    /* 0x94A */ s16                 field_94A;
    /* 0x94C */ s16                 field_94C;
    /* 0x94E */ s16                 field_94E;
    /* 0x950 */ s16                 field_950;
    /* 0x952 */ s16                 field_952;
    /* 0x954 */ u16                 field_954;
    /* 0x956 */ u16                 field_956;
    /* 0x958 */ s16                 field_958;
    /* 0x95A */ u16                 field_95A;
    /* 0x95C */ u16                 field_95C;
    /* 0x95E */ u16                 field_95E;
    /* 0x960 */ u16                 field_960;
    /* 0x962 */ u16                 field_962;
    /* 0x964 */ u16                 field_964; // previous field_962
    /* 0x966 */ u16                 field_966;
    /* 0x968 */ u16                 field_968; // released buttons: field_964 & ~field_962
    /* 0x96A */ u16                 field_96A; // set to 0xF89A by func_8010615C
    /* 0x96C */ s16                 field_96C;
    /* 0x96E */ s16                 field_96E;
    /* 0x970 */ s16                 field_970;
    /* 0x972 */ u8                  field_972;
    /* 0x973 */ s8                  field_973;
    /* 0x974 */ s8                  field_974;
    /* 0x975 */ s8                  field_975;
    /* 0x976 */ s8                  field_976;
    /* 0x977 */ s8                  field_977;
    /* 0x978 */ s8                  field_978;
    /* 0x979 */ s8                  field_979; // countdown; `func_mongoose_8011D1D8` loads 0xB
    /* 0x97A */ u8                  field_97A;
    /* 0x97B */ s8                  field_97B;
    /* 0x97C */ s8                  field_97C;
    /* 0x97D */ u8                  field_97D;
    /* 0x97E */ u8                  field_97E;
    /* 0x97F */ s8                  field_97F;
    /* 0x980 */ byte                pad_980;
    /* 0x981 */ u8                  field_981;
    /* 0x982 */ s8                  field_982;
    /* 0x983 */ u8                  field_983;
    /* 0x984 */ u8                  field_984;
    /* 0x985 */ u8                  field_985;
    /* 0x986 */ u8                  field_986;
    /* 0x987 */ u8                  field_987; // texture upload seq A (func_801030CC / D_80112E74)
    /* 0x988 */ u8                  field_988; // field_987 delay; reload 4 after each upload
    /* 0x989 */ u8                  field_989; // field_987 frame index
    /* 0x98A */ u8                  field_98A; // texture upload seq B (func_801030CC / D_80112EB4)
    /* 0x98B */ u8                  field_98B; // field_98A delay; reload 8 after each upload
    /* 0x98C */ u8                  field_98C; // field_98A frame index
    /* 0x98D */ u8                  field_98D;
    /* 0x98E */ u8                  field_98E;
    /* 0x98F */ s8                  field_98F; // cleared by Gp_SpawnWeaponEff
    /* 0x990 */ u8                  field_990;
    /* 0x991 */ s8                  field_991; // func_80109374 requires 0 to write field_97D = 1
    /* 0x992 */ u8                  field_992; // Gp_PlayerWorkState1: func_801011D0 result when field_984 & 1
    /* 0x993 */ u8                  field_993;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

/// The object at `Task::extra` for spawnType-1 tasks is a `TmdObject`
/// (`include/main/tmd.h`); it was previously modelled here as a truncated
/// `GameActorExt`.

// =============================================================================
// Globals
// =============================================================================

/// Pointer to the live `GameSession`.
extern GameSession* gGameSession;
extern GameSession  D61CC0_800714C0;

/// Session pointer-slot table on `gGameSession` (`ptrSlots`).
void         Game_SetPtrSlot(void* ptr, s32 index);
struct Task* Game_GetPtrSlot(s32 index);
void         Game_ClearPtrSlots(void);
void         Game_ClearSession(void);
void         Game_ClearEd68(void);

extern s32 D_8005ED68;
extern s32 D_8005ED8C;

#endif // SESSION_H
