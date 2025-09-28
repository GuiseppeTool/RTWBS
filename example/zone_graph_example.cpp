#include "rtwbs.h"
#include <iostream>
#include <vector>

using namespace rtwbs;

// This example loads a tiny TA and constructs the zone graph, printing
// basic statistics and states to sanity check invariant handling across Up.
int main() {
    try {
        // Use a minimal XML with: clock x; Inv(l0): x <= 5; edge guard x >= 10 (unreachable)
        const char* xml = R"(<?xml version="1.0"?>
<nta>
  <declaration>clock x; chan a;</declaration>
  <template>
    <name>T</name>
    <location id="l0">
      <name>L0</name>
      <label kind="invariant">x &lt;= 5</label>
    </location>
    <location id="l1">
      <name>L1</name>
    </location>
    <init ref="l0"/>
    <!-- Unreachable guard if invariants are enforced properly -->
    <transition>
      <source ref="l0"/>
      <target ref="l1"/>
      <label kind="guard">x &gt;= 10</label>
      <label kind="synchronisation">a!</label>
    </transition>
  </template>
  <system>process P = T(); system P;</system>
</nta>)";

        TimedAutomaton ta(xml);
        ta.construct_zone_graph();

        std::cout << "\nSanity check: initial states and transitions" << std::endl;
        ta.print_statistics();
        ta.print_all_states();
        ta.print_all_transitions();

        // Expectation: no transition should be taken to l1 because guard x>=10
        // contradicts invariant x<=5 under time elapse.
        if (ta.get_num_states() > 1) {
            std::cerr << "ERROR: Unreachable transition was explored (unexpected successor states)." << std::endl;
            return 2;
        }
        std::cout << "SUCCESS: No unreachable transitions explored." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
