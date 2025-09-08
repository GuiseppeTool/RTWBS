#include "rtwbs.h"
#include <iostream>

int main()
{
    std::cout << "Synchronization Example: Multi-Template Communication" << std::endl;
    std::cout << "====================================================" << std::endl;
    
    // XML content with multiple templates communicating via channels
    const char* xml_content = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE nta PUBLIC '-//Uppaal Team//DTD Flat System 1.6//EN' 'http://www.it.uu.se/research/group/darts/uppaal/flat-1_6.dtd'>
<nta>
    <declaration>
        // Global declarations
        clock x, y;
        chan start, done, timeout;
        int counter = 0;
    </declaration>
    
    <template>
        <name>Producer</name>
        <location id="prod_idle" x="0" y="0">
            <name x="-10" y="-34">Idle</name>
        </location>
        <location id="prod_working" x="144" y="0">
            <name x="134" y="-34">Working</name>
            <label kind="invariant" x="134" y="17">x&lt;=5</label>
        </location>
        <location id="prod_finished" x="288" y="0">
            <name x="278" y="-34">Finished</name>
            <label kind="invariant" x="278" y="17">y&lt;=2</label>
        </location>
        <init ref="prod_idle"/>
        
        <!-- Producer transitions -->
        <transition>
            <source ref="prod_idle"/>
            <target ref="prod_working"/>
            <label kind="synchronisation" x="42" y="-17">start?</label>
            <label kind="assignment" x="42" y="0">x:=0</label>
        </transition>
        <transition>
            <source ref="prod_working"/>
            <target ref="prod_finished"/>
            <label kind="guard" x="186" y="-17">x&gt;=3</label>
            <label kind="assignment" x="186" y="0">y:=0,counter:=counter+1</label>
        </transition>
        <transition>
            <source ref="prod_finished"/>
            <target ref="prod_idle"/>
            <label kind="synchronisation" x="144" y="34">done!</label>
            <label kind="guard" x="144" y="51">y&gt;=1</label>
        </transition>
        <transition>
            <source ref="prod_working"/>
            <target ref="prod_idle"/>
            <label kind="synchronisation" x="72" y="-51">timeout?</label>
            <label kind="guard" x="72" y="-68">x&gt;=4</label>
        </transition>
    </template>
    
    <template>
        <name>Consumer</name>
        <location id="cons_waiting" x="0" y="0">
            <name x="-10" y="-34">Waiting</name>
        </location>
        <location id="cons_requesting" x="144" y="0">
            <name x="134" y="-34">Requesting</name>
            <label kind="invariant" x="134" y="17">x&lt;=10</label>
        </location>
        <location id="cons_consuming" x="288" y="0">
            <name x="278" y="-34">Consuming</name>
            <label kind="invariant" x="278" y="17">y&lt;=3</label>
        </location>
        <init ref="cons_waiting"/>
        
        <!-- Consumer transitions -->
        <transition>
            <source ref="cons_waiting"/>
            <target ref="cons_requesting"/>
            <label kind="assignment" x="42" y="0">x:=0</label>
        </transition>
        <transition>
            <source ref="cons_requesting"/>
            <target ref="cons_consuming"/>
            <label kind="synchronisation" x="186" y="-17">done?</label>
            <label kind="assignment" x="186" y="0">y:=0</label>
        </transition>
        <transition>
            <source ref="cons_requesting"/>
            <target ref="cons_requesting"/>
            <label kind="synchronisation" x="144" y="-51">start!</label>
            <label kind="guard" x="144" y="-68">x&gt;=2</label>
        </transition>
        <transition>
            <source ref="cons_requesting"/>
            <target ref="cons_waiting"/>
            <label kind="synchronisation" x="72" y="34">timeout!</label>
            <label kind="guard" x="72" y="51">x&gt;=8</label>
        </transition>
        <transition>
            <source ref="cons_consuming"/>
            <target ref="cons_waiting"/>
            <label kind="guard" x="144" y="68">y&gt;=2</label>
        </transition>
    </template>
    
    <system>
        // System composition
        P = Producer();
        C = Consumer();
        system P, C;
    </system>
</nta>)";

    try {
        std::cout << "\n1. Parsing multi-template XML with synchronization..." << std::endl;
        
        // Create the timed automaton from XML
        rtwbs::TimedAutomaton automaton(xml_content);
        
        std::cout << "\n2. Analyzing synchronization channels..." << std::endl;
        std::cout << "   Expected channels: start, done, timeout" << std::endl;
        std::cout << "   Producer sends: done!" << std::endl;
        std::cout << "   Producer receives: start?, timeout?" << std::endl;
        std::cout << "   Consumer sends: start!, timeout!" << std::endl;
        std::cout << "   Consumer receives: done?" << std::endl;
        
        std::cout << "\n3. Constructing synchronized zone graph..." << std::endl;
        
        // Create initial zone (all clocks = 0)
        cindex_t dimension = automaton.get_dimension();
        std::vector<raw_t> initial_zone(dimension * dimension);
        dbm_init(initial_zone.data(), dimension);
        
        // Initial location for the composed system
        // In a multi-template system, this would be a composite state
        int initial_location = 0;
        
        std::cout << "   Starting from initial composite state" << std::endl;
        std::cout << "   Initial zone (all clocks = 0):" << std::endl;
        dbm_print(stdout, initial_zone.data(), dimension);
        std::cout << std::endl;
        
        // Construct the zone graph
        automaton.construct_zone_graph(initial_location, initial_zone);
        
        std::cout << "\n4. Synchronization analysis complete!" << std::endl;
        automaton.print_statistics();
        
        std::cout << "\n5. Reachable synchronized states:" << std::endl;
        automaton.print_all_states();
        
        std::cout << "\n6. Synchronization Analysis Summary:" << std::endl;
        std::cout << "   =================================" << std::endl;
        
        size_t num_states = automaton.get_num_states();
        std::cout << "   Total reachable states: " << num_states << std::endl;
        
        // Analyze reachability and synchronization patterns
        for (size_t i = 0; i < num_states; ++i) {
            const auto& successors = automaton.get_successors(i);
            if (!successors.empty()) {
                std::cout << "   State " << i << " has " << successors.size() 
                         << " successor(s) via synchronization" << std::endl;
            }
        }
        
        std::cout << "\n7. Synchronization Properties:" << std::endl;
        std::cout << "   - Producer-Consumer coordination via channels" << std::endl;
        std::cout << "   - Timeout mechanism for robustness" << std::endl;
        std::cout << "   - Clock constraints ensure timing properties" << std::endl;
        std::cout << "   - Counter tracks successful productions" << std::endl;
        
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
   
    return 0;
}
