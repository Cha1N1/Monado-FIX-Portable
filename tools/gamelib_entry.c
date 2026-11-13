// The one exported entry point of the recompiled game code library (runtime/gamelib.h).
// Built by tools/gamebuild.py together with the generated guest code, at first start
// from the player's own game files; the runtime loads the result and never contains
// game code itself.
#include "gamelib.h"

extern const uint64_t main_fn_addrs[], sdk_fn_addrs[], main_fn_count, sdk_fn_count;
extern const GuestFn main_fn_ptrs[], sdk_fn_ptrs[];

static const GameLibTables tables = {GAMELIB_ABI, GAMELIB_ID, main_fn_addrs, sdk_fn_addrs, main_fn_ptrs, sdk_fn_ptrs,
                                     &main_fn_count, &sdk_fn_count};

GAMELIB_EXPORT const GameLibTables *lasgame_tables(void) { return &tables; }
