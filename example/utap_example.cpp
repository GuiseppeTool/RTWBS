#include "utap/utap.hpp"
#include <iostream>
#include <sstream>

int main()
{
    std::cout << "UTAP Example" << std::endl;
    std::cout << "============" << std::endl;
    
    // Create a UTAP document
    UTAP::Document doc;
    
    // Example XML content for a simple timed automaton with parameters
    const char* xml_content = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE nta PUBLIC '-//Uppaal Team//DTD Flat System 1.6//EN' 'http://www.it.uu.se/research/group/darts/uppaal/flat-1_6.dtd'>
<nta>
    <declaration>clock x;</declaration>
    <template>
        <name>TestAutomaton</name>
        <parameter>const int pid</parameter>
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
    <instantiation>P1 := TestAutomaton(1);
P2 := TestAutomaton(2);</instantiation>
    <system>system P1, P2;</system>
</nta>)";

    std::cout << "Parsing XML buffer..." << std::endl;
    
    // Parse the XML content
    int32_t result = parse_XML_buffer(xml_content, doc, true);
    
    if (result == 0) {
        std::cout << "XML parsed successfully!" << std::endl;
        std::cout << "Document contains " << doc.get_templates().size() << " template(s)" << std::endl;
        
        // Check template parameters
        auto& templates = doc.get_templates();
        for (const auto& templ : templates) {
            std::cout << "  Template: " << templ.uid.get_name() << std::endl;
            std::cout << "    Unbound parameters: " << templ.unbound << std::endl;
            std::cout << "    Parameters frame size: " << templ.parameters.get_size() << std::endl;
        }
        
        // Check for processes
        auto& processes = doc.get_processes();
        std::cout << "Document contains " << processes.size() << " process(es)" << std::endl;
        
        for (const auto& process : processes) {
            std::cout << "  Process: " << process.uid.get_name() << std::endl;
            std::cout << "    Process parameters frame size: " << process.parameters.get_size() << std::endl;
            std::cout << "    Process unbound parameters: " << process.unbound << std::endl;
            std::cout << "    Process arguments: " << process.arguments << std::endl;
            
            // Check parameter mapping
            std::cout << "    Process parameter mapping:" << std::endl;
            for (const auto& [symbol, expression] : process.mapping) {
                std::cout << "      " << symbol.get_name() << " -> " << expression.str() << std::endl;
            }
        }
        
    } else {
        std::cout << "Parse failed with error code: " << result << std::endl;
    }
    

    
    return 0;
}