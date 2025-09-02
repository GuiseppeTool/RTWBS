#include "timedautomaton.h"
#include <iostream>

int main()
{
    std::cout << "Integration Example: XML Parsing to TimedAutomaton" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    // Enhanced XML content for a more complex timed automaton
    const char* xml_content = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE nta PUBLIC '-//Uppaal Team//DTD Flat System 1.6//EN' 'http://www.it.uu.se/research/group/darts/uppaal/flat-1_6.dtd'>
<nta>
    <declaration>clock x, y;int PIZZA = 5</declaration>
    <template>
        <name>TimerAutomaton</name>
        <location id="id0" x="0" y="0">
            <name x="-10" y="-34">Init</name>
        </location>
        <location id="id1" x="144" y="0">
            <name x="134" y="-34">Waiting</name>
            <label kind="invariant" x="134" y="17">x&lt;=10</label>
        </location>
        <location id="id2" x="288" y="0">
            <name x="278" y="-34">Done</name>
        </location>
        <init ref="id0"/>
        <transition>
            <source ref="id0"/>
            <target ref="id1"/>
            <label kind="guard" x="42" y="-17">x&gt;=false</label>
            <label kind="assignment" x="42" y="0">y:=PIZZA</label>
        </transition>
        <transition>
            <source ref="id1"/>
            <target ref="id2"/>
            <label kind="guard" x="186" y="-17">y&gt;=5</label>
        </transition>
        <transition>
            <source ref="id1"/>
            <target ref="id0"/>
            <label kind="guard" x="72" y="34">x&gt;=8+7</label>
            <label kind="assignment" x="72" y="51">x:=0</label>
        </transition>
    </template>
    <system>Process = TimerAutomaton();
system Process;</system>
</nta>)";

    try {
        // Create the timed automaton directly from XML
        rtwbs::TimedAutomaton automaton(xml_content);
        
        std::cout << "\n3. Constructing zone graph..." << std::endl;
        
        // Create initial zone (all clocks = 0)
        cindex_t dimension = automaton.get_dimension();
        std::vector<raw_t> initial_zone(dimension * dimension);
        dbm_init(initial_zone.data(), dimension);
        
        // Set initial location (first location, which should be the initial one)
        int initial_location = 0;
        
        std::cout << "   Starting from location " << initial_location << std::endl;
        std::cout << "   Initial zone:" << std::endl;
        dbm_print(stdout, initial_zone.data(), dimension);
        std::cout << std::endl;
        
        // Construct the zone graph
        automaton.construct_zone_graph(initial_location, initial_zone);
        
        std::cout << "\n4. Zone graph construction complete!" << std::endl;
        automaton.print_statistics();
        
        std::cout << "\n5. Printing all reachable states:" << std::endl;
        automaton.print_all_states();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
   
    return 0;
}
