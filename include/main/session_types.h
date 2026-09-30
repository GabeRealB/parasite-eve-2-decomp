#ifndef MAIN_SESSION_TYPES_H
#define MAIN_SESSION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation_types.h"
#include "gameplay/world_collision_types.h"

#include "main/coord.h"

struct GpActorD4;
struct AnimationRecord;
struct AnimationSet;
struct WorldTargetNode;
struct Task;

/// Location selector shared by the live session, saved state and world lookups.
///
/// The six unsigned bytes select parts of the location. `view` is
/// a room-local slot; a lookup maps it to the camera/image index. `variant`
/// selects the area's placement and resource layout and mirrors the saved
/// per-area variant after synchronization. Each layout supplies a placement
/// list and its associated resources.
///
/// Active stage, area, room, view and warp IDs are 1-based. Stage 0 means no
/// active stage; the other upper bounds depend on the selected loaded tables.
/// Each lookup requires only the components it reads to be initialized and in
/// range. The key is byte-aligned; packed word reads require a word-aligned
/// instance. Whole location-cell transfers use the eight-byte `GameLoc`.
typedef struct {
    u8 view;    // 1-based view slot within the room, mapped to a camera/image index
    u8 room;    // 1-based room within the area
    u8 area;    // 1-based area and CDF folder within the stage
    u8 stage;   // Stage ID (0 no active stage, 1..5 active stages)
    u8 warp;    // 1-based arrival record within the area's warp table
    u8 variant; // Placement/resource layout within the area (1 default)
} GameLocationKey;
STATIC_ASSERT_SIZEOF(GameLocationKey, 0x6);

/// Eight-byte location cell shared by the live session and saved state.
///
/// Whole-cell copies preserve the two bytes following the six-byte key;
/// their role is unproven. The cell is byte-aligned. Stream lookups borrow a
/// byte view of the cell and read only its leading view and room bytes;
/// a local copy can replace the view byte with a stream identifier.
typedef struct {
    GameLocationKey loc;          // Place key; leading view and room also select streams
    u8              unknown_6[2]; // Retained by whole-cell copies; role unproven
} GameLoc;
STATIC_ASSERT_SIZEOF(GameLoc, 8);

/// Music-loading and weapon-restoration options stored in `GameSession::flowFlags`.
enum {
    GAME_SESSION_FLOW_SKIP_ENDING_MUSIC      = 0x01,
    GAME_SESSION_FLOW_SKIP_AREA_MUSIC        = 0x02,
    GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY = 0x04,
    GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY   = 0x08,
    GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON = 0x40,
    GAME_SESSION_FLOW_REEQUIP_WEAPON         = 0x80
};

/// Pad-script activity and permission bits stored in `GameSession::padScriptFlags`.
enum {
    GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE          = 0x01,
    GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE          = 0x02,
    GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE = 0x80
};

/// Shared spawn-controller progress stored in `GameSession::spawnPhase`.
enum {
    GAME_SESSION_SPAWN_IDLE     = 0,
    GAME_SESSION_SPAWN_ARMED    = 1,
    GAME_SESSION_SPAWN_COMPLETE = 2
};

/// Incinerator descent progress retained across room and actor tasks.
enum {
    GAME_SESSION_INCINERATOR_DESCENT_WAITING  = 0,
    GAME_SESSION_INCINERATOR_DESCENT_MOVING   = 1,
    GAME_SESSION_INCINERATOR_DESCENT_LANDED   = 2,
    GAME_SESSION_INCINERATOR_DESCENT_COMPLETE = 3
};

/// Incinerator exit sequence selected by the warp or encounter controller.
enum {
    GAME_SESSION_INCINERATOR_EXIT_NONE      = 0,
    GAME_SESSION_INCINERATOR_EXIT_WARP      = 1,
    GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER = 2
};

/// Sentinels for the session's resource cache and death presentation.
enum {
    GAME_SESSION_CHARACTER_NOT_LOADED = -1,
    GAME_SESSION_DEATH_SOUND_HOLD     = 0x7F,
    GAME_SESSION_DEATH_FADE_DEFAULT   = -0x80
};

/// Established restart paths stored in `GameSession::restartMode`.
///
/// Value 2 is also written by a scripted death; its distinct role is unproven.
enum {
    GAME_SESSION_RESTART_NORMAL           = 0,
    GAME_SESSION_RESTART_COMPANION_1_DOWN = 1,
    GAME_SESSION_RESTART_PRESERVE_DISPLAY = 3,
    GAME_SESSION_RESTART_COMPANION_3_DOWN = 4,
    GAME_SESSION_RESTART_ENDING           = 0xFF
};

/// Live world, input and script state retained for one play session.
///
/// The resident instance behind `gGameSession` supplies shared state to main
/// and the overlays. New-game, load and reset paths clear the entire object;
/// view and room transitions update its location and request deferred reloads.
/// Task slots borrow their tasks and are cleared when the task system resets.
/// Input masks contain remapped, suppressed buttons rather than raw pad state.
/// Unknown storage is retained with its observed extent and is not padding.
typedef struct {
    s8           deathVariant;            // Death presentation choice (0 inactive, 1/2 file and sound variant)
    s8           eventState;              // Script-controlled state (0 idle, nonzero blocks direction/scene handling)
    u8           uiOpen;                  // UI overlay active (0 closed, 1 open); enables d-pad repeat
    byte         unknown_3;               // Role unproven
    GameLoc      location;                // Live place key and the two bytes retained with whole-cell copies
    struct Task* ptrSlots[16];            // Borrowed task anchors, slots 0..15; NULL when empty
    u8           applySaveVariant;        // Restore the saved placement variant on the next area load (0/1)
    u8           viewReady;               // View transition state (0 loading, 1 ready for scene tasks)
    u16          field_4E;                // Set to 1 by model loads and reflection setup; role unproven
    byte         unknown_50[2];           // Role unproven
    s16          viewDirty;               // Nonzero requests deferred respawn using the saved view
    byte         unknown_54[4];           // Role unproven
    u16          padHeld;                 // Remapped and suppressed held buttons, including analog directions
    u16          padPressed;              // Remapped and suppressed press edges for this frame
    u16          padReleased;             // Remapped and suppressed release edges for this frame
    u8           field_5E;                // Set to 1 when the play-clock task starts; role unproven
    u8           evtSkipped;              // Forced script skip (0/1); releases script and caption waits
    byte         unknown_60[4];           // Role unproven
    u8           freezeRoomObjs;          // Nonzero suppresses room-object state dispatch
    u8           sceneUpdatesPaused;      // Scene/HUD gate (0 updating, nonzero held); 1 also hides the target cursor
    u8           cutsceneHold;            // Scripted player/menu hold (0/1), separate from eventState
    u8           areaSetupDone;           // First arrival setup completed (0/1); later arrivals apply warp SFX/flags
    u8           hideHud;                 // Suppress HUD, item prompts and target cursor (0/1)
    u8           flowFlags;               // GAME_SESSION_FLOW_* music-loading and weapon-restoration bits
    byte         unknown_6A[0xA];         // Role unproven
    u16          spriteVariant;           // 1-based display resource variant; command packets retain its low byte
    s16          roomObjsDirty;           // Nonzero requests deferred room-object relinking
    s16          loadedStage;             // Stage whose resources were last queued (0 before a stage load)
    byte         unknown_7A[2];           // Role unproven
    s16          field_7C;                // Cleared when actor buffer 0 is reused; nonzero meaning unproven
    s16          field_7E;                // Cleared when actor buffer 1 is reused; nonzero meaning unproven
    s16          field_80;                // Cleared when actor buffer 2 is reused; nonzero meaning unproven
    byte         unknown_82[0x9A];        // Role unproven
    s16          loadedCharacterId;       // Character whose resources were last queued; -1 forces reload
    s16          loadedConfigSet;         // Last queued animation/model/file selector, paired with loadedCharacterId
    s16          sceneClock;              // Script frame countdown; arithmetic retains 16-bit wraparound
    s16          waterY;                  // Water-surface height in world-coordinate units
    u8           companionType;           // Cached companion resource family (0 none, 1/2/3 family)
    u8           companionVariant;        // Cached resource variant within companion family 1
    u8           battleResetPending;      // Battle/result reset handshake (0 cleared, 1 requested)
    u8           suppressDeathChecks;     // Nonzero suppresses player and companion death handling
    u8           restartMode;             // 0 ordinary, 1 companion-1 down, 2 unproven, 3 preserve display, 4 companion-3 down, 255 ending
    u8           loadedSndId;             // Last queued sound-file ID; 0 invalidates the cache
    s16          bossPartsHpSum;          // Cached 16-bit sum of boss-part HP, reused for successor HP scaling
    u8           suppressViewTriggers;    // Nonzero suppresses the player's view-transition collision checks
    s8           deathSoundCountdown;     // Death-sound countdown in ticks; 127 holds, a negative value triggers playback
    s8           deathFadeFrames;         // Death-fade duration in ticks; <=0 selects the 32-tick default
    u8           deathRestartDelay;       // Play-clock ticks before death presentation and restart
    u8           spawnPhase[2];           // Per-controller progress (0 idle, 1 armed, 2 complete)
    u8           incineratorDescentPhase; // Descent progress (0 waiting, 1 moving, 2 landed, 3 complete)
    byte         incineratorRoomGroup;    // Incinerator room family (0 rooms below 4, 1 rooms 4 and above)
    u8           eventRoomIndex;          // Zero-based event-reply room; replies add 1 (observed 0..6)
    u8           incineratorExitPhase;    // Exit sequence (0 none, 1 warp exit, 2 encounter exit)
    u8           enemyCullZone;           // Enemy-axis limit selector (0 disabled, 1..16 table entry)
    u8           skipEventIntro;          // Incinerator controller skips introductory spawns (0/1)
    byte         unknown_138;             // Set to 1 before a scripted stage transition; role unproven
    s8           hudShakeY;               // Upward HUD offset in units of 3 pixels; <=0 ignored
    u8           dirActionBusy;           // Direction/caption action in flight (0/1); blocks HUD actions
    u8           padScriptFlags;          // GAME_SESSION_PAD_SCRIPT_* activity and battle-freeze permission bits
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);
STATIC_ASSERT(OFFSET_OF(GameSession, location) == 4, GameSession_location);
STATIC_ASSERT(OFFSET_OF(GameSession, location.loc) == 4, GameSession_location_loc);
STATIC_ASSERT(OFFSET_OF(GameSession, ptrSlots) == 0xC, GameSession_ptrSlots);

/// The collision shape a body carries: the segment between two local
/// endpoints, the radius at each, and the table the contacts it makes are
/// recorded in. A body whose kind bits name this shape reaches it through its
/// `WorldCollisionBody` context.
///
/// Both endpoints are offsets under the body's own position in its own frame,
/// so the shape travels with it. The grid passes sweep the segment between the
/// two, and the pair passes test the cylinder that the segment and the two
/// radii describe, which is a capsule while the radii are equal and tapers
/// between them when they are not.
///
/// The table is the shape's link to its owner: it is sized and cleared when the
/// shape is set up, and the collision passes fill it as contacts are made. A
/// weapon re-arms the shape as it unfolds, pushing one endpoint out along its
/// reach and widening that end's radius with the spread.
///
/// Declared main-side because `GameActor` embeds one by value; the work blocks
/// that carry the same shape reach it through this declaration.
typedef struct {
    /* 0x00 */ SVECTOR                end0;       // one end of the segment, offset under the body
    /* 0x08 */ SVECTOR                end1;       // the other end
    /* 0x10 */ s16                    end0Radius; // radius at `end0`
    /* 0x12 */ s16                    end1Radius; // radius at `end1`; equal to `end0Radius` unless tapered
    /* 0x14 */ WorldCollisionContact* recs;       // table the contacts this body makes are recorded in
} GpActorD4Rec;
STATIC_ASSERT_SIZEOF(GpActorD4Rec, 0x18);

/// Large object pointed to by Task::work for the slot-3 game object
/// (gameGetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ s32                           field_0;  // per-frame X velocity (Gp_PlayerMode2State3)
    /* 0x004 */ s32                           field_4;  // per-frame Y velocity
    /* 0x008 */ s32                           field_8;  // per-frame Z velocity
    /* 0x00C */ byte                          pad_C[4];
    /* 0x010 */ s32                           field_10; // copy of GfxCoord.coord.t[0]
    /* 0x014 */ s32                           field_14; // copy of GfxCoord.coord.t[1]
    /* 0x018 */ s32                           field_18; // copy of GfxCoord.coord.t[2]
    /* 0x01C */ byte                          pad_1C[4];
    /* 0x020 */ s32                           field_20; // copied from Gp_SetActorDest arg2
    /* 0x024 */ s32                           field_24;
    /* 0x028 */ s32                           field_28;
    /* 0x02C */ byte                          pad_2C[4];
    /* 0x030 */ VECTOR                        field_30; // push-back dir (func_80109BB4); low halves go to a scratch SVECTOR
    /* 0x040 */ s32                           field_40;
    /* 0x044 */ s32                           field_44;
    /* 0x048 */ s32                           field_48;
    /* 0x04C */ byte                          pad_4C[4];
    /* 0x050 */ s16                           field_50; // SVECTOR.vx; func_80104D68 / RotMatrix
    /* 0x052 */ s16                           field_52; // facing angle (lh); func_8010BCF4 / func_80103E7C
    /* 0x054 */ s16                           field_54; // SVECTOR.vz; func_80104D68 / RotMatrix
    /* 0x056 */ byte                          pad_56[2];
    /* 0x058 */ s16                           field_58;
    /* 0x05A */ byte                          pad_5A[2];
    /* 0x05C */ s16                           field_5C; // pitch; Gp_AimPitchToLockAlt
    /* 0x05E */ byte                          pad_5E[2];
    /* 0x060 */ s16                           field_60;
    /* 0x062 */ byte                          pad_62[2];
    /* 0x064 */ s16                           field_64; // pitch; Gp_AimPitchToLockAlt
    /* 0x066 */ byte                          pad_66[2];
    /* 0x068 */ s16                           field_68;
    /* 0x06A */ s16                           field_6A;        // aim/look yaw offset; func_8010BE5C
    /* 0x06C */ byte                          pad_6C[4];
    /* 0x070 */ s16                           field_70;        // pitch-like angle; Gp_AimPitchRec
    /* 0x072 */ byte                          pad_72[6];
    /* 0x078 */ s16                           field_78;        // pitch; Gp_AimPitchDirect
    /* 0x07A */ byte                          pad_7A[6];
    /* 0x080 */ s16                           field_80;        // copied from func_80104F5C arg2
    /* 0x082 */ s16                           field_82;        // target facing angle; func_80104E00 / Gp_PlayerMode2State2
    /* 0x084 */ byte                          pad_84[4];
    /* 0x088 */ WorldCollisionMotionContext   field_88[3];     // `context.motion` of the `WorldCollisionBody` nodes at `field_AC`, `field_CC` and `field_EC`
    /* 0x0AC */ byte                          field_AC[0x20];  // first of the five `WorldCollisionBody` nodes at 0xAC..0x14C
    /* 0x0CC */ byte                          field_CC[0x20];  // `WorldCollisionBody` node
    /* 0x0EC */ byte                          field_EC[0x20];  // `WorldCollisionBody` node
    /* 0x10C */ byte                          field_10C[0x18]; // `WorldCollisionBody` node; `field_124` is its key
    /* 0x124 */ u32                           field_124;
    /* 0x128 */ byte                          pad_128[2];
    /* 0x12A */ u16                           field_12A;       // that node's flags
    /* 0x12C */ byte                          field_12C[0x20];
    /* 0x14C */ GpActorD4Rec                  field_14C;       // the actor's own collision shape; the weapon attach re-arms it
    /* 0x164 */ byte                          pad_164[0x18];
    /* 0x17C */ WorldCollisionContact         field_17C[18];   // Gp_ClearRec18Occupied / func_801041B4
    /* 0x32C */ WorldCollisionContact         field_32C[6];    // Gp_AttachActorObj / Gp_InitRec18Table
    /* 0x3BC */ WorldCollisionContact         aimContacts[1];  // Single result for the aiming capsule
    /* 0x3D4 */ GfxCoord                      field_3D4;       // copy of the attached model's root coordinate; the frame of the body at `field_10C`
    /* 0x424 */ byte                          field_424[0x14]; // AnimationContext overlay; Gp_AnimTickIndex
    /* 0x438 */ AnimationSlot                 field_438[19];   // the actor's animation slots, the array its `AnimationContext` walks
    /* 0x730 */ byte                          pad_730[0x10];
    /* 0x740 */ byte                          pad_740[0x68];
    /* 0x7A8 */ byte                          field_7A8; // addr taken as func_800B3F84 arg3
    /* 0x7A9 */ byte                          pad_7A9[0x163];
    /* 0x90C */ struct WorldTargetNode*       field_90C;
    /* 0x910 */ struct GpActorD4*             field_910;
    /* 0x914 */ struct Task*                  field_914;
    /* 0x918 */ struct Task*                  field_918;
    /* 0x91C */ struct Task*                  field_91C;
    /* 0x920 */ struct Task*                  field_920;
    /* 0x924 */ struct Task*                  field_924;
    struct AnimationSet**                     animationSets; // Borrowed set table used by the player's or companion's animation slots
    /* 0x92C */ const struct AnimationRecord* field_92C;     // last Gp_AnimGetRec result (Gp_PlayerNormalState5)
    /* 0x930 */ s32                           field_930;     // sw from Gp_MsgPlayerDirFacing; addr taken by func_801011D0
    /* 0x934 */ s32                           field_934;
    /* 0x938 */ s16                           field_938;     // number of animation slots the actor walks (init 0x13)
    /* 0x93A */ u16                           field_93A;     // Gp_WeaponIdBase[field_22-1] + field_21
    /* 0x93C */ u16                           field_93C;
    /* 0x93E */ s16                           field_93E;
    /* 0x940 */ s16                           field_940;
    /* 0x942 */ s16                           field_942;
    /* 0x944 */ s16                           field_944;
    /* 0x946 */ s16                           field_946;
    /* 0x948 */ s16                           field_948;
    /* 0x94A */ s16                           field_94A;
    /* 0x94C */ s16                           field_94C;
    /* 0x94E */ s16                           field_94E;
    /* 0x950 */ s16                           field_950;
    /* 0x952 */ s16                           field_952;
    /* 0x954 */ u16                           field_954;
    /* 0x956 */ u16                           field_956;
    /* 0x958 */ s16                           field_958;
    /* 0x95A */ u16                           field_95A;
    /* 0x95C */ u16                           field_95C;
    /* 0x95E */ u16                           field_95E;
    /* 0x960 */ u16                           field_960;
    /* 0x962 */ u16                           field_962;
    /* 0x964 */ u16                           field_964; // previous field_962
    /* 0x966 */ u16                           field_966;
    /* 0x968 */ u16                           field_968; // released buttons: field_964 & ~field_962
    /* 0x96A */ u16                           field_96A; // set to 0xF89A by func_8010615C
    /* 0x96C */ s16                           field_96C;
    /* 0x96E */ s16                           field_96E;
    /* 0x970 */ s16                           field_970;
    /* 0x972 */ u8                            field_972;
    /* 0x973 */ s8                            field_973;
    /* 0x974 */ s8                            field_974;
    /* 0x975 */ s8                            field_975;
    /* 0x976 */ s8                            field_976;
    /* 0x977 */ s8                            field_977;
    /* 0x978 */ s8                            field_978;
    /* 0x979 */ s8                            field_979; // countdown; `func_mongoose_8011D1D8` loads 0xB
    /* 0x97A */ u8                            field_97A;
    /* 0x97B */ s8                            field_97B;
    /* 0x97C */ s8                            field_97C;
    /* 0x97D */ u8                            field_97D;
    /* 0x97E */ s8                            field_97E;
    /* 0x97F */ s8                            field_97F;
    /* 0x980 */ byte                          pad_980;
    /* 0x981 */ s8                            field_981;
    /* 0x982 */ s8                            field_982;
    /* 0x983 */ u8                            field_983;
    /* 0x984 */ u8                            field_984;
    /* 0x985 */ u8                            field_985;
    /* 0x986 */ s8                            field_986;
    /* 0x987 */ u8                            field_987; // texture upload seq A (func_801030CC / D_80112E74)
    /* 0x988 */ u8                            field_988; // field_987 delay; reload 4 after each upload
    /* 0x989 */ u8                            field_989; // field_987 frame index
    /* 0x98A */ u8                            field_98A; // texture upload seq B (func_801030CC / D_80112EB4)
    /* 0x98B */ u8                            field_98B; // field_98A delay; reload 8 after each upload
    /* 0x98C */ u8                            field_98C; // field_98A frame index
    /* 0x98D */ u8                            field_98D;
    /* 0x98E */ u8                            field_98E;
    /* 0x98F */ s8                            field_98F; // cleared by Gp_SpawnWeaponEff
    /* 0x990 */ u8                            field_990;
    /* 0x991 */ s8                            field_991; // func_80109374 requires 0 to write field_97D = 1
    /* 0x992 */ u8                            field_992; // Gp_PlayerWorkState1: func_801011D0 result when field_984 & 1
    /* 0x993 */ u8                            field_993;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

#endif // MAIN_SESSION_TYPES_H
