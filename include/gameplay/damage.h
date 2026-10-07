#ifndef GAMEPLAY_DAMAGE_H
#define GAMEPLAY_DAMAGE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/enemy.h"

/// Computes weapon or Parasite Energy HP damage from a category-2 attack key.
///
/// Category 2 keys are accepted; other categories return 0 without a random
/// draw. The low seven bits select PE rows 0..54 when bit 15 is set, or
/// weapon rows 0..46 otherwise. Weapon keys also require a distance-scale row
/// in 0..46 in bits 8..13. These indices must be valid; no bounds checks run.
/// Each accepted key advances the shared random sequence once, even for zero
/// base damage. Companion and enemy attacks can also use this key encoding.
/// The victim applies HP loss and handles critical hits separately.
///
/// `playerDistance` is the attacker-to-target distance in world units, used
/// only for weapon attacks. Its distance/1000 bucket is narrowed to a byte
/// before selecting one of six distance classes. `scaledReaction` 0 disables
/// extra scaling; otherwise a matching weapon reaction uses `reactionScale`, a
/// nonnegative Q8 factor (256 is unity). PE attacks ignore both parameters.
///
/// Weapon damage rolls 100..119 percent of its base, then applies distance,
/// reaction and difficulty scales. Non-companion keys (bit 7 clear) with
/// rows below 33 also receive Berserker, Energy Shot and Skull Crystal boosts.
/// A nonzero weapon base deals at least 1 HP after scaling. PE damage rolls
/// 100..109 percent, receives Ofuda and difficulty scales, and may remain 0.
/// Life Drain rows 25..27 divide their base among the current PE targets;
/// zero targets gives zero damage. The current difficulty must be in 0..4.
u32 damageComputePlayerAttack(u32 attackKey, u32 playerDistance, s32 scaledReaction, s32 reactionScale);

/// Returns 1 when a player attack scores a critical hit on `enemy`, otherwise 0.
///
/// Requires live enemy parameters and coordinates. A weapon key has bit 15
/// clear, a low-seven-bit weapon row in 0..46, and a distance-scale row in
/// bits 8..13 in 0..46. Attachment keys, an absent player task, or a zero base
/// critical chance return 0 without a random draw. Eligible attacks compose
/// the enemy coordinate and consume one 12-bit draw.
///
/// Distance scales the chance, explosions use their own distance scale, and
/// bit 14 selects the alternate critical percentage. Buildup doubles the
/// chance and Energy Shot applies its current combo percentage.
/// `chanceMultiplier` is a factor, with 0 meaning no extra scaling; the chance
/// is not clamped, so a value of at least 4096 always succeeds.
///
/// The retained distance calculation rotates (low16 of body X, high16 of body
/// X, low16 of body Y) as signed components, ignores body Z, adds the enemy's
/// world translation, and measures from the player's world origin in game
/// units. Its distance/1000 bucket is narrowed to a halfword before lookup.
s32 damageRollCriticalHit(const Enemy* enemy, u32 attackKey, s32 chanceMultiplier);

/// Packs one enemy attack into a category-4 collision key for its victim.
///
/// `enemy` must be live. NULL parameters return the zero/no-contact key;
/// otherwise `param->attacks` must contain `attackIndex`, a nonnegative element
/// index. A NULL attack table is not accepted when the parameters exist.
/// The table is borrowed and unchanged. The low 12 power bits and low 4
/// reaction bits are packed; a zero-power attack still has category 4.
s32 damagePackEnemyAttackKey(const Enemy* enemy, s32 attackIndex);

/// Packs an attack-table entry into a category-4 collision key for its victim.
///
/// NULL `attacks` returns the zero/no-contact key. Otherwise `attackIndex` is
/// a nonnegative element index within the caller's table; no length is stored
/// or checked. The table is borrowed and unchanged. Power is masked to 12 bits
/// and reaction to 4 bits; a zero-power entry still has category 4.
s32 damagePackAttackKey(const DamageAttack* attacks, s32 attackIndex);

/// Credits a Life Drain hit to the current cast's pending player healing.
///
/// Call with the enemy's HP before subtracting the hit and a nonnegative
/// damage amount in HP. Low-seven-bit rows 25..27 identify Life Drain levels
/// 1..3; other rows do nothing. Category and attachment-selector bits are not
/// checked. Credit is the lesser of damage and remaining HP, compared as
/// unsigned words, and is added to `SceneCombatState.lifeDrainHp`. The cast
/// later pays that total to the player; this call changes no enemy HP.
/// `unused` is retained by the interface and ignored.
void damageAccumulateLifeDrainHp(const Enemy* enemy, s32 attackKey, s32 damage, s32 unused);

/// Victim whose fixed HP loss a category-5 hazard contact selects.
enum {
    DAMAGE_HAZARD_VICTIM_PLAYER = 0,
    DAMAGE_HAZARD_VICTIM_ENEMY  = 1,
};

/// Established weapon/attachment hit attributes; enemy kinds choose their response.
///
/// These are table attributes, separate from the category-4 victim reaction
/// packed by `damagePackAttackKey`. Other stored values have no established
/// shared interpretation here.
enum {
    DAMAGE_PLAYER_REACTION_NONE       = 0,
    DAMAGE_PLAYER_REACTION_STAGGER    = 1,
    DAMAGE_PLAYER_REACTION_BUILDUP    = 2,
    DAMAGE_PLAYER_REACTION_POISON     = 3,
    DAMAGE_PLAYER_REACTION_EXPLOSION  = 6,
    DAMAGE_PLAYER_REACTION_INCENDIARY = 7,
};

/// Returns the fixed HP damage a hazard contact deals to the selected victim.
///
/// The low halfword of `hazardKey` must select an existing hazard row (0..10);
/// category bits are ignored. Use `DAMAGE_HAZARD_VICTIM_PLAYER` or
/// `DAMAGE_HAZARD_VICTIM_ENEMY`; other selectors return 0 without a lookup.
/// This reads the tables without applying damage or retaining the key.
s32 damageGetHazardDamage(s32 hazardKey, s32 victim);

/// Returns a player attack's unsigned reaction/special attribute widened to s32.
///
/// Bit 15 selects attachment rows; the low seven bits select row 0..54 there
/// or 0..46 in the weapon table. All other bits, including category, are
/// ignored. Requires an attack row: healing rows share this column with their
/// upper heal amount. This reads the table without changing reaction state.
s32 damageGetPlayerAttackReaction(s32 attackKey);

/// Returns the unsigned hit-effect kind of a player attack.
///
/// Bit 15 selects attachment rows (0..54), otherwise weapon rows (0..46);
/// the low seven bits must select an existing row. Other bits are ignored.
/// Zero selects no hit effect. This only reads the table; it spawns no effect.
u16 damageGetPlayerAttackEffectId(s32 attackKey);

/// Returns the hit cooldown/stun duration of a player attack, in frames.
///
/// Bit 15 selects attachment rows (0..54), otherwise weapon rows (0..46);
/// the low seven bits must select an existing row. Other bits are ignored.
/// The stored u16 is widened to s32; zero imposes no duration. The victim
/// decides whether to arm a cooldown or a stun; this call changes no timer.
s32 damageGetPlayerAttackHitCooldown(s32 attackKey);

/// Requests an enemy's stagger reaction without changing its timers.
///
/// Requires a live enemy; its actor consumes and clears the reaction flag.
void damageStartEnemyStagger(Enemy* enemy);

/// Restarts an enemy's buildup reaction and its two byte counters.
///
/// Requires a live enemy. Weapon keys use grade 0. Attachment keys use the
/// active cast's level digit (1..3), except low-six-bit row 49, which uses 0.
/// Category and other row bits are ignored; the level is not read from the
/// key. The actor owns ticking and clearing the reaction. `unused` is ignored.
void damageStartEnemyBuildup(Enemy* enemy, s32 attackKey, s32 unused);

/// Rolls an enemy's poison chance and restarts damage over time on success.
///
/// Requires live enemy parameters. Always consumes one 12-bit random draw;
/// success consumes another and resets the pulse count and delay to 83..98
/// frames. Failure leaves the enemy unchanged. The enemy kind's percent
/// chance is unclamped: 0 never succeeds and 100 or more always does.
/// Weapon keys use grade 0; attachment keys use the active cast's level digit
/// (1..3), independent of the key's row/category. `unused` is ignored.
void damageTryStartEnemyDamageOverTime(Enemy* enemy, s32 attackKey, s32 unused);

/// Advances damage over time by one frame and returns this frame's HP damage.
///
/// Requires live parameters and a started reaction with grade 0..3; the flag
/// is not checked. A due pulse advances the byte pulse count, reseeds its delay
/// to 83..98 frames and returns the grade's percentage of maximum HP, at least
/// 1 HP. Other frames return 0. A byte delay of 0 wraps and waits 256 calls
/// before a pulse. The actor applies HP loss and checks expiry.
s32 damageTickEnemyDamageOverTime(Enemy* enemy);

/// Returns 1 when an enemy's damage-over-time reaction is absent or expired.
///
/// Requires live parameters and grade 0..3 when the flag is set. A zero base
/// pulse count never expires; otherwise the grade scales the base count and
/// expiry compares it against pulses already dealt. This changes no state
/// and clears no flags. The byte pulse count wraps, so a scaled limit above
/// 255 cannot be reached.
s32 damageIsEnemyDamageOverTimeExpired(const Enemy* enemy);

/// Advances an enemy's buildup reaction by one frame; returns 1 at its end.
///
/// Requires live parameters, a started reaction and grade 0..3. The grade
/// scales the base number of 31-frame steps; a zero base never completes.
/// The last step consumes one draw for a 0..63-frame byte countdown, which
/// is decremented on later calls. A zero draw wraps and completes after 256
/// calls. A scaled step limit above 255 cannot be reached by the byte counter.
/// The flag is not checked or cleared; the actor stops ticking on return 1.
s32 damageTickEnemyBuildup(Enemy* enemy);

#endif // GAMEPLAY_DAMAGE_H
