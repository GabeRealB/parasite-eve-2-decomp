#include "main/random.h"

#include "gameplay/room_effects.h"

/* Part of the Glutton library; see glutton.h. */

/// Scratch-stack block holding the two records a Glutton hit effect is spawned from.
///
/// Both are arguments of the id-dispatched effect spawner. Reserve one
/// complete block and release it once the effect is spawned; nothing in it is
/// meant to outlive the call.
typedef struct {
    SVECTOR        offset;    // Where the effect appears: an offset from the struck body's coordinate, along that coordinate's axes
    EffectSpawnArg effectArg; // Spawner's argument record: the struck body's coordinate, with spawn-argument halves 0x500 and 3
} _GluttonHitEffectScratch;
STATIC_ASSERT_SIZEOF(_GluttonHitEffectScratch, 0x10);

/// Spawns a player attack's hit effect relative to the struck coordinate.
///
/// `attackKey` must select a valid weapon or attachment attack row; category bits
/// are ignored. Reactions 2, 4, 6 and 7 use a fixed offset; other values choose
/// one of three offsets with one LCG advance. Offsets use model units along
/// `coord`'s local axes. Requires a live coordinate and initialized scratch with
/// one `_GluttonHitEffectScratch` plus the spawner's nested capacity. The records
/// are released before return; the effects must not rely on a live offset pointer
/// into that released storage.
static void _gluttonHitEffect(GfxCoord* coord, s32 attackKey)
{
    enum { GLUTTON_HIT_EFFECT_SIZE  = 0x500,
           GLUTTON_HIT_EFFECT_COUNT = 3 };
    _GluttonHitEffectScratch* scratch = SCRATCH_STACK_RESERVE_BLOCK(_GluttonHitEffectScratch);

    scratch->effectArg.spawnArgLo = GLUTTON_HIT_EFFECT_SIZE;
    scratch->effectArg.coord      = coord;
    scratch->effectArg.spawnArgHi = GLUTTON_HIT_EFFECT_COUNT;

    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {
        case DAMAGE_PLAYER_REACTION_BUILDUP:
        case 4:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
            scratch->offset.vx = 0;
            scratch->offset.vy = -0x190;
            scratch->offset.vz = 0x258;
            effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
            break;
        case DAMAGE_PLAYER_REACTION_NONE:
        case DAMAGE_PLAYER_REACTION_STAGGER:
        case DAMAGE_PLAYER_REACTION_POISON:
        case 5:
        case 8:
        case 9:
        default:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            switch ((u16)((gRandomLcgState >> 16) % 3U)) {
                case 0:
                    scratch->offset.vy = 0;
                    scratch->offset.vx = 0;
                    scratch->offset.vz = 0x384;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 1:
                    scratch->offset.vx = 0x258;
                    scratch->offset.vy = -0xC8;
                    scratch->offset.vz = 0x2BC;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 2:
                    scratch->offset.vx = -0x12C;
                    scratch->offset.vy = -0x320;
                    scratch->offset.vz = 0x320;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GluttonHitEffectScratch);
}
