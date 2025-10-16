#include "lily.h"
#include <iostream>

int main() {
    std::cout << "=== LV2 Plugin Information Test ===" << std::endl;
    
    // Get all LV2 plugins with effect types
    json plugins = lily_getLv2Json();
    
    std::cout << "\nFound " << plugins.size() << " LV2 plugins:" << std::endl;
    std::cout << "========================================" << std::endl;
    
    for (auto& [key, plugin] : plugins.items()) {
        std::cout << "Plugin: " << plugin["name"] << std::endl;
        std::cout << "  URI: " << plugin["uri"] << std::endl;
        std::cout << "  Effect Type: " << plugin["effect_type"] << std::endl;
        std::cout << "  Class: " << plugin["class_label"] << std::endl;
        std::cout << "  Class URI: " << plugin["class_uri"] << std::endl;
        std::cout << "  ----------------------------------------" << std::endl;
    }
    
    // Test detailed plugin info for the first plugin (if any exist)
    if (!plugins.empty()) {
        auto first_plugin = plugins.begin().value();
        std::string plugin_uri = first_plugin["uri"];
        
        std::cout << "\n=== Detailed Info for: " << first_plugin["name"] << " ===" << std::endl;
        json detailed_info = lily_getPluginInfo(plugin_uri.c_str());
        
        std::cout << detailed_info.dump(2) << std::endl;
    }
    
    return 0;
}