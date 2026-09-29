/* CAP caption relocation, rendering, timing and schedule playback.
 *
 * ShowTimed and LoadResource are static-inline implementations behind ordinary
 * overlay entry points. Helpers and common settings are private. The including
 * overlay defines its own schedule descriptor and caption work at their data
 * positions. Cross-TU work declarations belong in the overlay's private header;
 * exported schedule declarations belong in its public header. Retained caret
 * bytes stay with that instance's storage. No configuration switches are needed.
 *
 * Include this header in the prologue and the _settings and _schedule storage
 * fragments at their data positions. At the function run, include
 * cap_captions.inc.c, define the ShowTimed wrapper, include
 * cap_captions_resource.inc.c, then define the LoadResource wrapper.
 */

#ifndef SRC_SHARED_CAP_CAPTIONS_H
#define SRC_SHARED_CAP_CAPTIONS_H

#include "types.h"

#include "shared/cap_caption_types.h"

static inline void CapCaption_ShowTimed(s16 arg0, s16 arg1, s16 arg2);
static inline void CapCaption_LoadResource(s16 arg0, s16 arg1, s16 arg2);

#endif /* SRC_SHARED_CAP_CAPTIONS_H */
