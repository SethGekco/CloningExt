#pragma once

// Cross-hook runtime state.
//
// ProducingExtras is set while our dispatch layer is kicking its own extra
// clones. Two hooks read it:
//   * the dispatch hooks, to stop our own kicks from recursing into more extras;
//   * the clone-init hook, to recognise a unit we just cloned as a clone (so it
//     gets the reduced HP / veterancy) even when it exits the very factory that
//     produced the primary (the "factory that also clones" / Fix-1 case).
#include <vector>

class TechnoClass;

namespace CloningExt
{
	inline bool ProducingExtras = false;
	// Produced units flagged Clone.RemoveOriginal, deleted next logic frame
	// (0x55B4E1); purged by AnnounceInvalidPointer (0x7258D0) if they die first.
	inline std::vector<TechnoClass*> RemoveQueue;
}
