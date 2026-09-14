#include <cassert>
#include <iostream>

#include "tb/crane_presentation.h"

int main()
{
    using tb::CranePresentationMode;

    // House.u() starts K=4.  While the first block is raising/attached (or
    // otherwise not in state 2), House.e(Graphics) uses mesh 7 + cable.
    assert(tb::crane_presentation_mode(true, 0, false, false, false) == CranePresentationMode::Special);

    // Releasing the very first block sets c[0]==2 while K remains 4: no crane
    // mesh is drawn until the first successful landing changes K to 0.
    assert(tb::crane_presentation_mode(true, 0, false, true, false) == CranePresentationMode::Hidden);

    // After the first floor has landed, normal construction uses mesh 8.
    assert(tb::crane_presentation_mode(true, 1, false, false, false) == CranePresentationMode::Normal);

    // House.e(Graphics) explicitly selects mesh 7 whenever c[0]==3 (missed),
    // even after the initial floor.
    assert(tb::crane_presentation_mode(true, 5, false, false, true) == CranePresentationMode::Special);

    // Build City roof K==1 always uses mesh 7 + the separate Java2D cable.
    assert(tb::crane_presentation_mode(true, 9, true, true, false) == CranePresentationMode::Special);

    assert(tb::crane_presentation_mode(false, 5, false, false, false) == CranePresentationMode::Hidden);

    // L-family mesh mapping recovered from House resource loading.
    assert(tb::normal_floor_mesh_id(1) == 10);
    assert(tb::normal_floor_mesh_id(4) == 13);
    assert(tb::initial_base_mesh_id(1) == 20);
    assert(tb::initial_base_mesh_id(4) == 23);

    std::cout << "crane presentation ok\n";
}
