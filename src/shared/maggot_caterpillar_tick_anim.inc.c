/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Out-of-line form of `_maggotCaterpillarTickAnimInline`: switches the work's animation
/// id, or ticks every slot one frame when it is unchanged.
void maggotCaterpillarTickAnim(Task* arg0)
{
    _maggotCaterpillarTickAnimInline(arg0);
}
