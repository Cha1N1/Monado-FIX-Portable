// T266: middle-button stick click = a pulse on the press, dropped while ZL is held (camera stuck behind the player).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../runtime/input_bind.h"

int main(void) {
    XcdeStickClick s = {0};
    // plain click: one pulse start, nothing while it stays held, nothing on release
    assert(xcde_stick_click_edge(&s, true, false) == 1);
    assert(xcde_stick_click_edge(&s, true, false) == 0);
    assert(xcde_stick_click_edge(&s, false, false) == -1);  // (release: the pulse ends)
    memset(&s, 0, sizeof s);
    // ZL first, then the click: dropped for the whole press, and ZL released first does not bring it back
    assert(xcde_stick_click_edge(&s, false, true) == 0);
    assert(xcde_stick_click_edge(&s, true, true) == 0);
    assert(xcde_stick_click_edge(&s, true, false) == 0);
    assert(xcde_stick_click_edge(&s, false, false) == 0);
    // next press (no ZL) works again
    assert(xcde_stick_click_edge(&s, true, false) == 1);
    // click first, ZL pressed during the pulse: the pulse is cut, ZL released does not restart it
    assert(xcde_stick_click_edge(&s, true, true) == -1);
    assert(xcde_stick_click_edge(&s, true, false) == 0);
    assert(xcde_stick_click_edge(&s, false, false) == 0);
    puts("test_stick_click: ok");
    return 0;
}
