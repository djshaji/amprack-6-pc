#include "lily.h"
#include <string>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include "util.h"

void generateLV2Info (std::string homedir) {
    IN
    json j = {}, categories = {}, creators = {};
    categories ["All"] = std::vector <int> () ;
    creators ["All"] = std::vector <int> () ;

    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);

    const LilvPlugins* list = lilv_world_get_all_plugins(world);
    int x = 1 ;

    LILV_FOREACH (plugins, i, list) {
        const LilvPlugin* p = lilv_plugins_get(list, i);
        const char * name = lilv_node_as_string (lilv_plugin_get_name (p));
        const char * uri = lilv_node_as_string (lilv_plugin_get_uri (p));
        const LilvNodes* features = lilv_plugin_get_supported_features (p);
        
        // Get plugin classes to determine effect type
        const LilvPluginClass* plugin_class = lilv_plugin_get_class(p);
        const char* class_uri = lilv_node_as_uri(lilv_plugin_class_get_uri(plugin_class));
        const char* class_label = lilv_node_as_string(lilv_plugin_class_get_label(plugin_class));
        LilvNode* author_node = lilv_plugin_get_author_name(p);
        std::string author_name_str = "Unknown";
        if (author_node) author_name_str = lilv_node_as_string(author_node);

        LilvNode* email_node = lilv_plugin_get_author_email(p);
        std::string author_email_str = "";
        if (email_node) author_email_str = lilv_node_as_string(email_node);

        LilvNode* homepage_node = lilv_plugin_get_author_homepage(p);
        std::string author_homepage_str = "";
        if (homepage_node) author_homepage_str = lilv_node_as_string(homepage_node);

        // Populate creators map
        if (creators.find(author_name_str) == creators.end()) {
            creators[author_name_str] = std::vector<int>();
        }
        creators[author_name_str].push_back(x);
        // creators["All"].push_back(x);

        // Free nodes
        if (author_node) lilv_node_free(author_node);
        if (email_node) lilv_node_free(email_node);
        if (homepage_node) lilv_node_free(homepage_node);
        
        // Determine effect type based on plugin class
        std::string effect_type = "Unknown";
        if (strstr(class_uri, "DelayPlugin")) {
            effect_type = "Delay";
        } else if (strstr(class_uri, "ReverbPlugin")) {
            effect_type = "Reverb";
        } else if (strstr(class_uri, "DistortionPlugin")) {
            effect_type = "Distortion";
        } else if (strstr(class_uri, "FilterPlugin")) {
            effect_type = "Filter";
        } else if (strstr(class_uri, "EQPlugin")) {
            effect_type = "EQ";
        } else if (strstr(class_uri, "CompressorPlugin")) {
            effect_type = "Compressor";
        } else if (strstr(class_uri, "LimiterPlugin")) {
            effect_type = "Limiter";
        } else if (strstr(class_uri, "ModulatorPlugin")) {
            effect_type = "Modulator";
        } else if (strstr(class_uri, "ChorusPlugin")) {
            effect_type = "Chorus";
        } else if (strstr(class_uri, "FlangerPlugin")) {
            effect_type = "Flanger";
        } else if (strstr(class_uri, "PhaserPlugin")) {
            effect_type = "Phaser";
        } else if (strstr(class_uri, "GeneratorPlugin")) {
            effect_type = "Generator";
        } else if (strstr(class_uri, "InstrumentPlugin")) {
            effect_type = "Instrument";
        } else if (strstr(class_uri, "UtilityPlugin")) {
            effect_type = "Utility";
        } else if (strstr(class_uri, "AnalyserPlugin")) {
            effect_type = "Analyser";
        } else if (strstr(class_uri, "AmplifierPlugin")) {
            effect_type = "Amplifier";
        } else if (strstr(class_uri, "SpatialPlugin")) {
            effect_type = "Spatial";
        } else if (strstr(class_uri, "SpectralPlugin")) {
            effect_type = "Spectral";
        } else if (class_label) {
            effect_type = class_label;
        }
        
        LOGD ("[LV2] %s -> %s [Class: %s, Type: %s]\n", name, uri, class_uri, effect_type.c_str());
            // Populate categories
        if (categories.find(effect_type) == categories.end()) {
            categories[effect_type] = std::vector<int>();
        }

        categories[effect_type].push_back(x);

        json plugin = {};
        plugin ["name"] = name ;
        plugin ["uri"] = uri ;
        plugin ["type"] = "lv2" ;
        plugin ["effect_type"] = effect_type ;
        plugin ["class_uri"] = class_uri ;
        plugin ["class_label"] = class_label ? class_label : "Unknown" ;
        plugin ["index"] = 0 ;
        plugin ["id"] = x ;
        plugin ["library"] = uri ;

        std::string idx = std::to_string(x);
        j[idx] = plugin;
        x++ ;
    }

    lilv_world_free(world);

    LOGD ("LV2 plugins found: %s\n", j.dump ().c_str ());
    json_to_filename (j, homedir + "/lv2_plugins.json");
    json_to_filename (categories, homedir + "/lv2_categories.json");
    json_to_filename (creators, homedir + "/lv2_creators.json");
    
    OUT
}

std::string lily_getEffectType (const LilvPlugin* plugin) {
    IN
    
    const LilvPluginClass* plugin_class = lilv_plugin_get_class(plugin);
    const char* class_uri = lilv_node_as_uri(lilv_plugin_class_get_uri(plugin_class));
    const char* class_label = lilv_node_as_string(lilv_plugin_class_get_label(plugin_class));
    
    // Map LV2 plugin classes to effect types
    if (strstr(class_uri, "DelayPlugin")) {
        return "Delay";
    } else if (strstr(class_uri, "ReverbPlugin")) {
        return "Reverb";
    } else if (strstr(class_uri, "DistortionPlugin")) {
        return "Distortion";
    } else if (strstr(class_uri, "FilterPlugin")) {
        return "Filter";
    } else if (strstr(class_uri, "EQPlugin")) {
        return "EQ";
    } else if (strstr(class_uri, "CompressorPlugin")) {
        return "Compressor";
    } else if (strstr(class_uri, "LimiterPlugin")) {
        return "Limiter";
    } else if (strstr(class_uri, "ModulatorPlugin")) {
        return "Modulator";
    } else if (strstr(class_uri, "ChorusPlugin")) {
        return "Chorus";
    } else if (strstr(class_uri, "FlangerPlugin")) {
        return "Flanger";
    } else if (strstr(class_uri, "PhaserPlugin")) {
        return "Phaser";
    } else if (strstr(class_uri, "GeneratorPlugin")) {
        return "Generator";
    } else if (strstr(class_uri, "InstrumentPlugin")) {
        return "Instrument";
    } else if (strstr(class_uri, "UtilityPlugin")) {
        return "Utility";
    } else if (strstr(class_uri, "AnalyserPlugin")) {
        return "Analyser";
    } else if (strstr(class_uri, "AmplifierPlugin")) {
        return "Amplifier";
    } else if (strstr(class_uri, "SpatialPlugin")) {
        return "Spatial";
    } else if (strstr(class_uri, "SpectralPlugin")) {
        return "Spectral";
    } else if (class_label) {
        return std::string(class_label);
    }
    
    OUT
    return "Unknown";
}

json lily_getPluginInfo (const char* plugin_uri) {
    IN
    json plugin_info = {};
    
    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);
    
    LilvNode* uri_node = lilv_new_uri(world, plugin_uri);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(lilv_world_get_all_plugins(world), uri_node);
    
    if (plugin) {
        const char* name = lilv_node_as_string(lilv_plugin_get_name(plugin));
        const char* author_name = NULL;
        const char* author_email = NULL;
        const char* author_homepage = NULL;
        
        // Get author information
        LilvNode* author_node = lilv_plugin_get_author_name(plugin);
        if (author_node) {
            author_name = lilv_node_as_string(author_node);
        }
        
        LilvNode* email_node = lilv_plugin_get_author_email(plugin);
        if (email_node) {
            author_email = lilv_node_as_string(email_node);
        }
        
        LilvNode* homepage_node = lilv_plugin_get_author_homepage(plugin);
        if (homepage_node) {
            author_homepage = lilv_node_as_string(homepage_node);
        }
        
        // Get plugin class and effect type
        std::string effect_type = lily_getEffectType(plugin);
        const LilvPluginClass* plugin_class = lilv_plugin_get_class(plugin);
        const char* class_uri = lilv_node_as_uri(lilv_plugin_class_get_uri(plugin_class));
        const char* class_label = lilv_node_as_string(lilv_plugin_class_get_label(plugin_class));
        
        // Get port information
        uint32_t num_ports = lilv_plugin_get_num_ports(plugin);
        json ports = json::array();
        
        for (uint32_t i = 0; i < num_ports; i++) {
            const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
            LilvNode* port_name = lilv_port_get_name(plugin, port);
            
            json port_info = {};
            port_info["index"] = i;
            port_info["name"] = port_name ? lilv_node_as_string(port_name) : "Unknown";
            
            // Determine port type
            if (lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_CORE__AudioPort))) {
                port_info["type"] = "audio";
            } else if (lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_CORE__ControlPort))) {
                port_info["type"] = "control";
            } else if (lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_ATOM__AtomPort))) {
                port_info["type"] = "atom";
            } else {
                port_info["type"] = "unknown";
            }
            
            // Determine port direction
            if (lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_CORE__InputPort))) {
                port_info["direction"] = "input";
            } else if (lilv_port_is_a(plugin, port, lilv_new_uri(world, LV2_CORE__OutputPort))) {
                port_info["direction"] = "output";
            }
            
            // Get control port ranges
            if (port_info["type"] == "control") {
                LilvNode* min_node, *max_node, *def_node;
                lilv_port_get_range(plugin, port, &def_node, &min_node, &max_node);
                
                if (def_node) port_info["default"] = lilv_node_as_float(def_node);
                if (min_node) port_info["minimum"] = lilv_node_as_float(min_node);
                if (max_node) port_info["maximum"] = lilv_node_as_float(max_node);
            }
            
            ports.push_back(port_info);
            
            if (port_name) lilv_node_free(port_name);
        }
        
        // Build complete plugin info
        plugin_info["name"] = name;
        plugin_info["uri"] = plugin_uri;
        plugin_info["effect_type"] = effect_type;
        plugin_info["class_uri"] = class_uri;
        plugin_info["class_label"] = class_label ? class_label : "Unknown";
        plugin_info["num_ports"] = num_ports;
        plugin_info["ports"] = ports;
        
        if (author_name) plugin_info["author_name"] = author_name;
        if (author_email) plugin_info["author_email"] = author_email;
        if (author_homepage) plugin_info["author_homepage"] = author_homepage;
        
        // Free author nodes
        if (author_node) lilv_node_free(author_node);
        if (email_node) lilv_node_free(email_node);
        if (homepage_node) lilv_node_free(homepage_node);
    }
    
    lilv_node_free(uri_node);
    lilv_world_free(world);
    
    OUT
    return plugin_info;
}