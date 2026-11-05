/* The strips the Antibody and Apobiosis effects draw between two points. */
#ifndef PE_EFFECTS_STRIP_STRIP_H
#define PE_EFFECTS_STRIP_STRIP_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <gameplay/effects.h>

/// Projects the strip's end point when `projectEnd` is true, otherwise its start point.
static inline void _effectStripProjectPoint(EffectStripScratch* scratch, bool projectEnd)
{
    if (projectEnd) {
        gte_ldv0(&scratch->worldEnd);
        gte_rtps();
        gte_stsxy(&scratch->screenEnd);
        gte_stflg(&scratch->projectionFlags);
    } else {
        gte_ldv0(&scratch->worldStart);
        gte_rtps();
        gte_stsxy(&scratch->screenStart);
        gte_stflg(&scratch->projectionFlags);
    }
}

#endif /* PE_EFFECTS_STRIP_STRIP_H */
