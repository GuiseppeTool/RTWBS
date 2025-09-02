#include "utap/utap.hpp"
#include <iostream>
#include <sstream>

int main()
{
    std::cout << "UTAP Example" << std::endl;
    std::cout << "============" << std::endl;
    
    // Create a UTAP document
    UTAP::Document doc;
    
    // Example XML content for a simple timed automaton
    const char* xml_content = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE nta PUBLIC '-//Uppaal Team//DTD Flat System 1.6//EN' 'http://www.it.uu.se/research/group/darts/uppaal/flat-1_6.dtd'>
<nta>
    <declaration>clock x;</declaration>
    <template>
        <name>TestAutomaton</name>
        <location id="id0" x="0" y="0">
            <name x="-10" y="-34">Init</name>
        </location>
        <location id="id1" x="144" y="0">
            <name x="134" y="-34">Target</name>
        </location>
        <init ref="id0"/>
        <transition>
            <source ref="id0"/>
            <target ref="id1"/>
            <label kind="guard" x="42" y="-17">x&gt;=1</label>
        </transition>
    </template>
    <system>Process = TestAutomaton();
system Process;</system>
</nta>)";

    std::cout << "Parsing XML buffer..." << std::endl;
    
    // Parse the XML content
    int32_t result = parse_XML_buffer(xml_content, doc, true);
    
    if (result == 0) {
        std::cout << "XML parsed successfully!" << std::endl;
        std::cout << "Document contains " << doc.get_templates().size() << " template(s)" << std::endl;
        
    } else {
        std::cout << "Parse failed with error code: " << result << std::endl;
    }
    

    
    return 0;
}