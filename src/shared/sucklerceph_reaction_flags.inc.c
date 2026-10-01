/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Per-frame reaction dispatch of the first enemy, on its `reactionFlags`:
/// bit 0x1 counts the death frames and grows the scale factor, killing the
/// enemy on the fifth; bit 0x2 is consumed and moves the reaction state to 3
/// with the step and the rebind stopped; bits 0xC tick the flag-4 helper,
/// feed the damage it reports to `sucklercephTakeDamage` and are cleared once it
/// expires.
void sucklercephReactionFlags(Task* arg0)
{
    SucklercephWork* work;
    Enemy*           enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = (SucklercephWork*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            work->field_2D4 += 1;
            work->field_2AC += 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                sucklercephKill(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = 2;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= 0xFD;
            work->field_2B2       = 3;
            work->field_2B6       = 0;
            work->field_2BE       = 0;
            work->field_2D2       = 1;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            tick = Gp_TickObjFlag4(enemy);
            if (tick != 0) {
                sucklercephTakeDamage(arg0, tick);
            }
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}
