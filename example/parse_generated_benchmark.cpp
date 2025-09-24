#include <iostream>
#include <string>
#include "rtwbs.h"

int main(int argc, char* argv[]) {
    try {
        std::string path;
        if (argc >= 2) {
            path = argv[1];
        } else {
            std::cerr << "Usage: " << argv[0] << " <path-to-uppaal-xml>\n";
            std::cerr << "Example: " << argv[0] << " /home/luca/Documents/TimedAutomata/AA_BENCHMARKS/suites/bench_0.xml\n";
            return 2;
        }
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: File not found: " << path << std::endl;
            return 1;
        }
        //if path ends with .xml run this code
        if (path.size() < 4 || path.substr(path.size() - 4) == ".xml") {
            rtwbs::System sys(path);
            sys.print_system_overview();
            sys.construct_all_zone_graphs();
            sys.print_all_statistics();
            return 0;
        // otherwise, we assume its a folder and we try to parse all xml files in it
        } else {
            std::vector<std::string> files;
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".xml") {
                    files.push_back(entry.path().string());
                }
            }
            if (files.empty()) {
                std::cerr << "Error: No .xml files found in directory: " << path << std::endl;
                return 1;
            }
            for (const auto& file : files) {
                std::cout << "Processing file: " << file << std::endl;
                try {
                    rtwbs::System sys(file);
                    //sys.print_system_overview();
                    sys.construct_all_zone_graphs();
                    sys.print_all_statistics();
                } catch (const std::exception& e) {
                    std::cerr << "Error processing file '" << file << "': " << e.what() << std::endl;
                }
            }
            return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
