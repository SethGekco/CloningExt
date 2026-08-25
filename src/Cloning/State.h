#pragma once

// Cross-hook runtime state.
//
// ProducingExtras is set while our dispatch layer is kicking its own extra
// clones. Two hooks read it:
//   * the dispatch hooks, to stop our own kicks from recursing into more extras;
//   * the clone-init hook, to recognise a unit we just cloned as a clone (so it
//     gets the reduced HP / veterancy) even when it exits the very factory that
//     produced the primary (the "factory that also clones" / Fix-1 case).
namespace CloningExt
{
	inline bool ProducingExtras = false;
}
