#include "choice_state.h"
#include <cassert>
#include <iostream>
int main() {
    ChoiceState s;
    assert(!s.move(1));
    assert(!s.set("Invalid", {"One"}, {}));
    assert(s.set("Question", {"First", "Second", "Third"}, {"id-4", "id-9", "id-2"}));
    assert(s.move(-1) && s.selected == 2 && s.ids[s.selected] == "id-2");
    assert(s.move(1) && s.selected == 0);
    assert(s.move(1) && s.ids[s.selected] == "id-9");
    auto generation = s.generation;
    s.clear();
    assert(!s.active && s.ids.empty() && s.generation != generation && !s.move(1));
    assert(s.set("Next", {"Only"}, {"new-id"}) && s.selected == 0);
    assert(s.current().find("Answer 1 of 1. Only") != std::string::npos);
    std::cout << "Choice mapping, wrapping, reset and stale-state checks passed.\n";
}
