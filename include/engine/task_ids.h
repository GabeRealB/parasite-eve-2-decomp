#ifndef ENGINE_TASK_IDS_H
#define ENGINE_TASK_IDS_H

/// The task banks `taskSpawn` selects by number: each a resident table of descriptors, named by what it holds, and
/// listed together in `gTaskDescBanks`. Every bank from 1 on opens with four teardown slots. A bank named by its
/// number holds nothing whose role is known: banks 3, 5 and 14 hold only the development image's tasks and are never
/// spawned by number, banks 8 and 10 a copy of the burst body part some enemy families spawn, and banks 11..13 are
/// bank 2's table again, spawned by nothing. Which bank a package spawns from is compiled into it: nothing registers a
/// table or reads the bank directory but the spawn itself.
enum {
    TASK_BANK_SYSTEM      = 0, // Session flow, boot, the title, views, loading, memory cards, the play clock
    TASK_BANK_SCENE       = 1, // The scene manager, lights, fades, item pickups, reflections, enemy teardown
    TASK_BANK_INTERACTION = 2, // CAP playback, pad scripts, rooms' devices
    TASK_BANK_3           = 3,
    TASK_BANK_COLLISION   = 4, // World collision
    TASK_BANK_5           = 5,
    TASK_BANK_EFFECT      = 6, // Gameplay's effects, by their ids (`effect_ids.h`)
    TASK_BANK_MODEL       = 7, // Model tasks: the player's bodies, child models, weapon shells, actors' models
    TASK_BANK_8           = 8,
    TASK_BANK_EVENT       = 9, // Event scripts, CAP control, object-task rooms
    TASK_BANK_10          = 10,
    TASK_BANK_11          = 11,
    TASK_BANK_12          = 12,
    TASK_BANK_13          = 13,
    TASK_BANK_14          = 14,
    TASK_BANK_COUNT       = 15,
};

/// One task's id over all banks: its bank in bits 16..30 and its slot in the low halfword, which `effectSpawn` takes
/// (and an effect is a task of the effect bank). Neither part is masked; each argument is evaluated once, and constant
/// arguments make a constant expression. `EFFECT_SPAWN_UNLIMITED` may be ORed in.
#define TASK_ID(bank, slot) (((bank) << 16) | (slot))

/// A task id's slot in its bank, for spawning it with `taskSpawn`.
#define TASK_SLOT(id) ((id) & 0xFFFF)

// Each bank's slots, named for the task its descriptor runs; a teardown stub, an empty slot and a development-image
// task have none. Where a task sits in several slots of a bank, the later is numbered. The effect bank's slots are its
// effect ids (`effect_ids.h`).

/// `TASK_BANK_SYSTEM`'s slots (`_gSystemTaskDescs`).
enum {
    TASK_SYSTEM_NOOP                  = 0,
    TASK_SYSTEM_COUNTDOWN             = 1,
    TASK_SYSTEM_TITLE_SCREEN          = 2,
    TASK_SYSTEM_LAUNCH_SESSION        = 3,
    TASK_SYSTEM_LOAD_DIALOG           = 4,
    TASK_SYSTEM_VIBRATION_OPTIONS     = 5,
    TASK_SYSTEM_TITLE_EXIT            = 6,
    TASK_SYSTEM_START_SESSION         = 9,
    TASK_SYSTEM_CARD_SAVE_DIALOG      = 10,
    TASK_SYSTEM_CARD_LOAD_DIALOG      = 11,
    TASK_SYSTEM_BOOT_START_TITLE      = 13,
    TASK_SYSTEM_VIEW_APPLY_COORD      = 14,
    TASK_SYSTEM_VIEW_APPLY_CAMERA     = 15,
    TASK_SYSTEM_LOAD_ROOM_RESOURCES   = 16,
    TASK_SYSTEM_RELOAD_SESSION        = 17,
    TASK_SYSTEM_CARD_SAVE_DIALOG_2    = 18,
    TASK_SYSTEM_CARD_LOAD_DIALOG_2    = 19,
    TASK_SYSTEM_DIRECTION             = 20,
    TASK_SYSTEM_VIEW_TRANSITION_GATE  = 22,
    TASK_SYSTEM_VIEW_CACHED_PACKETS   = 23,
    TASK_SYSTEM_EXIT_CALLBACK         = 24,
    TASK_SYSTEM_DRAW_TEMPORARY_MODELS = 26,
    TASK_SYSTEM_SPRITE_VIEW           = 27,
    TASK_SYSTEM_LOAD_SESSION          = 28,
    TASK_SYSTEM_PLAY_CLOCK            = 29,
    TASK_SYSTEM_LOAD_VIEW             = 30,
    TASK_SYSTEM_COLD_START            = 31,
    TASK_SYSTEM_RELOAD_TITLE          = 32,
    TASK_SYSTEM_RESUME_SESSION_FADE   = 33,
    TASK_SYSTEM_VIEW_COMMIT_INDEX     = 37,
    TASK_SYSTEM_RESTORE_VIEW_GRAPHICS = 38,
    TASK_SYSTEM_SLOT_COUNT            = 39,
};

/// `TASK_BANK_SCENE`'s slots (`gSceneTaskDescs`).
enum {
    TASK_SCENE_ENEMY_TEARDOWN_DELAY    = 11,
    TASK_SCENE_ROOM_LIGHTS             = 15,
    TASK_SCENE_PLAYER_LIGHTING         = 16,
    TASK_SCENE_FADE_PULSE              = 25,
    TASK_SCENE_RELEASE_MODEL_BUFFERS   = 33,
    TASK_SCENE_RESTORE_MODEL_BUFFERS   = 34,
    TASK_SCENE_SCENE_MANAGER           = 35,
    TASK_SCENE_NOOP                    = 36,
    TASK_SCENE_PLAYER_REFLECTION       = 37,
    TASK_SCENE_ITEM_PICKUP             = 38,
    TASK_SCENE_DISPLAY_TRANSITION_FADE = 39,
    TASK_SCENE_ITEM_PICKUP_PROMPT      = 44,
    TASK_SCENE_BLEND_PREVIOUS_FRAME    = 45,
    TASK_SCENE_RUN_EXIT_CALLBACK       = 46,
    TASK_SCENE_SCREEN_FADE             = 49,
    /// A burst body part: publish its `TmdSource` in this slot's `data.model` just before spawning it.
    TASK_SCENE_BURST_BODY_PART = 50,
    TASK_SCENE_SLOT_COUNT      = 51,
};

/// `TASK_BANK_INTERACTION`'s slots (`gInteractionTaskDescs`), which banks 11..13 share.
enum {
    /// The burst body part's task with no body of its own; nothing is known to spawn it.
    TASK_INTERACTION_BURST_BODY_PART_BODYLESS = 6,
    TASK_INTERACTION_CAP_PLAYBACK             = 7,
    TASK_INTERACTION_AKROPOLIS_BRIDGE_KEYPAD  = 8,
    TASK_INTERACTION_SECURITY_ROOM_MONITOR    = 9,
    TASK_INTERACTION_SECURITY_ROOM_POWER      = 10,
    TASK_INTERACTION_PAD_MOTOR_HOLD           = 11,
    TASK_INTERACTION_PAD_MOTOR_RAMP           = 12,
    TASK_INTERACTION_PAD_SCRIPT               = 13,
    TASK_INTERACTION_AKROPOLIS_FOUNTAIN_CLIMB = 14,
    TASK_INTERACTION_LANDING_PAD_EXIT         = 15,
    /// A burst body part, at the actors' priority: publish its `TmdSource` in this slot's `data.model` first.
    TASK_INTERACTION_BURST_BODY_PART = 16,
    TASK_INTERACTION_SLOT_COUNT      = 17,
};

/// `TASK_BANK_COLLISION`'s slots (`gCollisionTaskDescs`).
enum {
    TASK_COLLISION_WORLD_COLLISION_UPDATE = 5,
    /// A burst body part: publish its `TmdSource` in this slot's `data.model` just before spawning it.
    TASK_COLLISION_BURST_BODY_PART = 7,
    TASK_COLLISION_SLOT_COUNT      = 9,
};

/// `TASK_BANK_5`'s slots (`gTaskBank5Descs`).
enum {
    TASK_5_DEBUG_LAUNCH = 1,
    TASK_5_SLOT_COUNT   = 5,
};

/// `TASK_BANK_MODEL`'s slots (`gModelTaskDescs`): runs of model tasks, each found from its first by an index - a
/// costume, a weapon, a launcher, a variant - and a few single models.
enum {
    /// Aya's body in each costume: Akropolis, Dryfield, Mist, Range.
    TASK_MODEL_PLAYER_BODIES = 4,
    /// Aya's attachment groups, four models each.
    TASK_MODEL_AYA_ATTACHMENTS = 9,
    /// Aya's weapons, the first being weapon 1's.
    TASK_MODEL_AYA_WEAPONS = 64,
    /// The grenade projectiles, four a launcher.
    TASK_MODEL_GRENADE_PROJECTILES = 0x60,
    TASK_MODEL_HYPERVELOCITY_SLIDE = 112,
    TASK_MODEL_HYPERVELOCITY_HINGE = 116,
    /// The companion's models, armed and not, each found as a base and a variant from 1.
    TASK_MODEL_COMPANION_ARMED_BASE = 0x7F,
    TASK_MODEL_COMPANION_OTHER_BASE = 0x82,
    /// Kyle's attachment groups, four models each.
    TASK_MODEL_KYLE_ATTACHMENTS = 135,
    /// Kyle's weapons, the first being weapon 1's.
    TASK_MODEL_KYLE_WEAPONS          = 155,
    TASK_MODEL_CAFETERIA_LOOSE_PROPS = 160,
    TASK_MODEL_SLOT_COUNT            = 164,
};

/// `TASK_BANK_8`'s slots (`gTaskBank8Descs`).
enum {
    /// A burst body part: publish its `TmdSource` in this slot's `data.model` just before spawning it.
    TASK_8_BURST_BODY_PART = 5,
    TASK_8_SLOT_COUNT      = 6,
};

/// `TASK_BANK_EVENT`'s slots (`gEventTaskDescs`).
enum {
    TASK_EVENT_CAP_CONTROL            = 6,
    TASK_EVENT_INTERPRETER            = 7,
    TASK_EVENT_CAP_HUD_SLIDE          = 8,
    TASK_EVENT_CAP_ACTION_PROMPT_EXIT = 11,
    TASK_EVENT_SCREEN_SHAKE           = 12,
    TASK_EVENT_MUSIC_VOLUME_FADE      = 13,
    TASK_EVENT_SOUND_ATTENUATION_FADE = 14,
    TASK_EVENT_OBJECT_TASK_ROOM       = 17,
    TASK_EVENT_SLOT_COUNT             = 19,
};

/// `TASK_BANK_10`'s slots (gameplay's `gTaskBank10Descs`).
enum {
    /// A burst body part: publish its `TmdSource` in this slot's `data.model` just before spawning it.
    TASK_10_BURST_BODY_PART = 5,
    TASK_10_SLOT_COUNT      = 6,
};

#endif // ENGINE_TASK_IDS_H
