#ifndef INCLUDE_KYLE_KYLE_H
#define INCLUDE_KYLE_KYLE_H

#include "gameplay/animation.h"

#include "main/tmd_types.h"

/// Kyle Madigan's native companion animation bank for resource variant 1.
///
/// Saved variants 0 and 1 select this bank. The first
/// `ANIMATION_BANK_BASE_SET_COUNT` entries are package-owned clips or NULL;
/// the following `ANIMATION_BANK_EXTENSION_CAPACITY` words start zeroed and
/// remain writable for borrowed scripted clips and other raw words. Play an
/// extension entry only when it holds a valid set pointer. Body playback uses
/// tracks 1..19 of the twenty-track clips. Keep this package and any copied
/// clip data loaded while playback or cached records borrow it.
extern AnimationSet* gKyle800101KyleMadiganAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Kyle Madigan's native companion animation bank for resource variant 3.
///
/// The first `ANIMATION_BANK_BASE_SET_COUNT` entries are package-owned clips
/// or NULL. The following `ANIMATION_BANK_EXTENSION_CAPACITY` words start
/// zeroed and remain writable for borrowed scripted clips and other raw words;
/// play an extension entry only when it holds a valid set pointer.
/// Body playback uses tracks 1..19 of the twenty-track clips. Keep this package
/// and any copied clip data loaded while playback or cached records borrow it.
extern AnimationSet* gKyle800103KyleMadiganAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Kyle Madigan's twenty-part companion body for resource variant 1.
///
/// Borrows this package's geometry and writable command stream. Keep the
/// package loaded until all models using this source have been released.
extern TmdSource gKyle800101Body;

/// Kyle Madigan's one-part right-hand model for resource variant 1.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800101HandRight;

/// Kyle Madigan's one-part right hand in attachment pair 1 for resource variant 1.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800101HandRight3;

/// Kyle Madigan's one-part left hand in attachment pair 0 for resource variant 1.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800101HandLeft2;

/// Kyle Madigan's one-part left hand in attachment pair 1 for resource variant 1.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// The held weapon model attaches beneath this hand's root.
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800101HandLeft;

/// Kyle Madigan's twenty-part companion body for resource variant 3.
///
/// Borrows this package's geometry and writable command stream. Keep the
/// package loaded until all models using this source have been released.
extern TmdSource gKyle800103Body;

/// Kyle Madigan's one-part right-hand model for resource variant 3.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800103HandRight;

/// Kyle Madigan's one-part right hand in attachment pair 1 for resource variant 3.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800103HandRight3;

/// Kyle Madigan's one-part left hand in attachment pair 0 for resource variant 3.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800103HandLeft2;

/// Kyle Madigan's one-part left hand in attachment pair 1 for resource variant 3.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// The held weapon model attaches beneath this hand's root.
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800103HandLeft;

/// Kyle Madigan's twenty-part companion body for resource variant 4.
///
/// Borrows this package's geometry and writable command stream. Keep the
/// package loaded until all models using this source have been released.
extern TmdSource gKyle800104Body;

/// Kyle Madigan's one-part right-hand model for resource variant 4.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800104HandRight;

/// Kyle Madigan's one-part right hand in attachment pair 1 for resource variant 4.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800104HandRight3;

/// Kyle Madigan's one-part left hand in attachment pair 0 for resource variant 4.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800104HandLeft2;

/// Kyle Madigan's one-part left hand in attachment pair 1 for resource variant 4.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// The held weapon model attaches beneath this hand's root.
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800104HandLeft;

/// Kyle Madigan's twenty-part companion body for resource variant 2.
///
/// Borrows this package's geometry and writable command stream. Keep the
/// package loaded until all models using this source have been released.
extern TmdSource gKyle800102Body;

/// Kyle Madigan's one-part right-hand model for resource variant 2.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800102HandRight;

/// Kyle Madigan's one-part right hand in attachment pair 1 for resource variant 2.
///
/// Parent its root to the companion body's right-hand joint (part 12).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800102HandRight3;

/// Kyle Madigan's one-part left hand in attachment pair 0 for resource variant 2.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800102HandLeft2;

/// Kyle Madigan's one-part gun model for companion resource variant 1.
///
/// Parent its root to the companion's left-hand model. Borrows this package's
/// geometry and writable command stream; keep the package loaded until every
/// model using this source has been released.
extern TmdSource gKyleP229;

/// Kyle Madigan's one-part M950 model for companion resource variant 3.
///
/// Parent its root to the companion's left-hand model. Borrows this package's
/// geometry and writable command stream; keep the package loaded until every
/// model using this source has been released.
extern TmdSource gKyleM950;

/// Kyle Madigan's one-part left hand in attachment pair 1 for resource variant 2.
///
/// Parent its root to the companion body's left-hand joint (part 8).
/// The held weapon model attaches beneath this hand's root.
/// Borrows this package's geometry and writable command stream; keep the
/// package loaded until every model using this source has been released.
extern TmdSource gKyle800102HandLeft;

/// Kyle Madigan's one-part gun model for companion resource variant 4.
///
/// Parent its root to the companion's left-hand model. Borrows this package's
/// geometry and writable command stream; keep the package loaded until every
/// model using this source has been released.
extern TmdSource gKyleM4a1;

/// Kyle Madigan's native companion animation bank for resource variant 4.
///
/// The first `ANIMATION_BANK_BASE_SET_COUNT` entries are package-owned clips
/// or NULL. The following `ANIMATION_BANK_EXTENSION_CAPACITY` words start
/// zeroed and remain writable for borrowed scripted clips and other raw words;
/// play an extension entry only when it holds a valid set pointer.
/// Body playback uses tracks 1..19 of the twenty-track clips. Keep this package
/// and any copied clip data loaded while playback or cached records borrow it.
extern AnimationSet* gKyle800104KyleMadiganAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Kyle Madigan's one-part MM1 launcher for companion resource variant 2.
///
/// Parent its root to the companion's left-hand model. Its root supplies the
/// coordinate frame for muzzle effects and shell launch offsets.
/// Borrows this package's geometry and writable command stream;
/// keep the package loaded until every model using this source is released.
extern TmdSource gKyleMm1;

/// Grenade-shell model fired by Kyle Madigan's MM1 launcher.
///
/// Launch parents its single root to the weapon's coordinate frame, then
/// reparents it to the world. Borrows this package's geometry and writable
/// command stream; keep the package loaded until every shell model is released.
extern TmdSource gKyleMm1GrenadeShell;

#endif // INCLUDE_KYLE_KYLE_H
