#include "types.h"

#include "gameplay/attachment_state.h"
#include "hud.h"
#include "game_debug_types.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
HudHpMp gHudDisplayedVitals;

u8 gAttachmentTrainingLevels[ATTACHMENT_ABILITY_COUNT];

u16 gGameDebugReplayButtons;

u16 gGameDebugReplayFramesRemaining;

AttachmentState gAttachmentState;

InventoryItemRow gInventoryBattleRewardRows[5];

s32 gAttachmentSoundQueueTicks;

const GameDebugReplayRecord* gGameDebugReplayCursor;

#include "attachments.h"
#include "game_debug.h"

#include "gameplay/attachments.h"
#include "gameplay/damage.h"
#include "gameplay/room_effects.h"

/// Unhandled hit-effect kinds retained by this catalog; both currently spawn nothing.
enum { ATTACHMENT_LEVEL_NO_HIT_EFFECT          = 0,
       ATTACHMENT_LEVEL_ENERGY_BALL_HIT_EFFECT = 14 };

AttachmentLevelTable gAttachmentLevelTable = { .rows = {
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 8, 22, 70, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 2 } },
                                                   { .column = { 1250, 1, 7, 22, 100, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 2 } },
                                                   { .column = { 3000, 2, 6, 22, 45, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 2 } },
                                                   { .column = { 750, 1, 14, 38, 120, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 0 } },
                                                   { .column = { 1750, 1, 14, 30, 140, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 0 } },
                                                   { .column = { 4000, 2, 14, 22, 170, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST, 0 } },
                                                   { .column = { 3000, 1, 30, 50, 200, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST_WITH_SOUND, 0 } },
                                                   { .column = { 4000, 2, 30, 40, 250, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST_WITH_SOUND, 0 } },
                                                   { .column = { 5000, 4, 30, 28, 300, { .hitReaction = DAMAGE_PLAYER_REACTION_INCENDIARY }, EFFECT_HIT_KIND_BLAST_WITH_SOUND, 0 } },
                                                   { .column = { 500, 1, 7, 38, 15, { .hitReaction = DAMAGE_PLAYER_REACTION_POISON }, EFFECT_HIT_KIND_TINTED_PUFF, 12 } },
                                                   { .column = { 1250, 1, 7, 30, 20, { .hitReaction = DAMAGE_PLAYER_REACTION_POISON }, EFFECT_HIT_KIND_TINTED_PUFF, 12 } },
                                                   { .column = { 3000, 2, 7, 22, 30, { .hitReaction = DAMAGE_PLAYER_REACTION_POISON }, EFFECT_HIT_KIND_TINTED_PUFF, 12 } },
                                                   { .column = { 750, 1, 6, 18, 25, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, EFFECT_HIT_KIND_SPARK_BURST, 0 } },
                                                   { .column = { 1750, 1, 5, 18, 30, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, EFFECT_HIT_KIND_SPARK_BURST, 0 } },
                                                   { .column = { 4000, 2, 4, 18, 40, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, EFFECT_HIT_KIND_SPARK_BURST, 0 } },
                                                   { .column = { 3000, 1, 18, 38, 60, { .hitReaction = DAMAGE_PLAYER_REACTION_BUILDUP }, EFFECT_HIT_KIND_APOBIOSIS_SHARD, 0 } },
                                                   { .column = { 4000, 2, 18, 38, 80, { .hitReaction = DAMAGE_PLAYER_REACTION_BUILDUP }, EFFECT_HIT_KIND_APOBIOSIS_SHARD, 0 } },
                                                   { .column = { 5000, 4, 18, 38, 100, { .hitReaction = DAMAGE_PLAYER_REACTION_BUILDUP }, EFFECT_HIT_KIND_APOBIOSIS_SHARD, 0 } },
                                                   { .column = { 500, 1, 7, 36, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 1250, 1, 5, 28, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 3000, 1, 3, 18, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 750, 1, 12, 42, 30, { .healMax = 60 }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 1750, 1, 12, 30, 40, { .healMax = 70 }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 4000, 1, 12, 18, 50, { .healMax = 90 }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 3000, 1, 20, 44, 200, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, 0 } },
                                                   { .column = { 4000, 1, 18, 36, 250, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, 0 } },
                                                   { .column = { 5000, 2, 16, 28, 300, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, 0 } },
                                                   { .column = { 500, 2, 6, 36, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 1250, 2, 5, 28, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 3000, 4, 4, 18, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 750, 2, 10, 36, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 1750, 2, 9, 28, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 4000, 4, 8, 18, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 3000, 2, 15, 36, 200, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, ATTACHMENT_LEVEL_ENERGY_BALL_HIT_EFFECT, 0 } },
                                                   { .column = { 4000, 4, 15, 28, 200, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, ATTACHMENT_LEVEL_ENERGY_BALL_HIT_EFFECT, 0 } },
                                                   { .column = { 5000, 9, 15, 18, 200, { .hitReaction = DAMAGE_PLAYER_REACTION_STAGGER }, ATTACHMENT_LEVEL_ENERGY_BALL_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 5, 5, 30, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 4, 10, 30, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 3, 20, 30, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 50, { .hitReaction = 9 }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 12, 5, { .hitReaction = DAMAGE_PLAYER_REACTION_BUILDUP }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 18, 0, { .hitReaction = 8 }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                                   { .column = { 0, 0, 0, 0, 0, { .hitReaction = DAMAGE_PLAYER_REACTION_NONE }, ATTACHMENT_LEVEL_NO_HIT_EFFECT, 0 } },
                                               } };

u16 attachmentGetActiveLevelValue(s32 column)
{
    enum {
        ATTACHMENT_ACTIVE_LEVEL_MIN          = 1,
        ATTACHMENT_ACTIVE_LEVEL_MAX          = ATTACHMENT_LEVEL_COUNT,
        ATTACHMENT_TRAINING_RESOURCE_VARIANT = 4
    };
    const PlayerStatus* playerStatus;
    s32                 usesTrainingLevels;
    s32                 level;
    const u8*           learnedLevels;
    s32                 abilityIndex;
    const u8*           levelTableBytes;
    s32                 valueOffsetBytes;

    levelTableBytes = gAttachmentLevelTable.bytes;
    abilityIndex    = gAttachmentState.activeIndex;
    if (abilityIndex >= ATTACHMENT_SPELL_COUNT) {
        level = ATTACHMENT_ACTIVE_LEVEL_MIN;
    } else {
        playerStatus = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            usesTrainingLevels = 0;
        } else {
            usesTrainingLevels = playerStatus->resourceVariant == ATTACHMENT_TRAINING_RESOURCE_VARIANT;
        }
        if (usesTrainingLevels == 0) {
            learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
        } else {
            learnedLevels = gAttachmentTrainingLevels;
        }
        level = learnedLevels[abilityIndex];
        if (level == 0) {
            level = ATTACHMENT_ACTIVE_LEVEL_MIN;
        }
        if (playerStatus->statusFlags & PLAYER_STATUS_BERSERKER) {
            if (level < ATTACHMENT_ACTIVE_LEVEL_MAX) {
                level++;
            }
        }
    }
    // Select one aligned halfword; column scaling precedes row scaling.
    valueOffsetBytes  = column * sizeof(u16);
    valueOffsetBytes += (gAttachmentState.activeIndex * ATTACHMENT_ACTIVE_LEVEL_MAX + level) * sizeof(AttachmentLevelRow);
    return *(const u16*)(levelTableBytes + valueOffsetBytes);
}
