#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lilv/lilv.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <lv2/log/log.h>

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 512
#define NUM_FRAMES 1024

// Simple URID map implementation
typedef struct {
    char** uris;
    size_t n_uris;
} URIDMap;

static LV2_URID urid_map(LV2_URID_Map_Handle handle, const char* uri) {
    URIDMap* map = (URIDMap*)handle;
    
    // Check if URI already exists
    for (size_t i = 0; i < map->n_uris; i++) {
        if (strcmp(map->uris[i], uri) == 0) {
            return i + 1;
        }
    }
    
    // Add new URI
    map->uris = realloc(map->uris, sizeof(char*) * (map->n_uris + 1));
    map->uris[map->n_uris] = strdup(uri);
    return ++map->n_uris;
}

static const char* urid_unmap(LV2_URID_Unmap_Handle handle, LV2_URID urid) {
    URIDMap* map = (URIDMap*)handle;
    if (urid == 0 || urid > map->n_uris) {
        return NULL;
    }
    return map->uris[urid - 1];
}

// Simple logger implementation
static int logger_printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[LV2 Log] ");
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}

// Plugin host structure
typedef struct {
    LilvWorld* world;
    const LilvPlugin* plugin;
    LilvInstance* instance;
    
    // Features
    URIDMap urid_map;
    LV2_URID_Map map;
    LV2_URID_Unmap unmap;
    LV2_Log_Log log;
    
    LV2_Feature map_feature;
    LV2_Feature unmap_feature;
    LV2_Feature log_feature;
    const LV2_Feature* features[4];
    
    // Port information
    uint32_t n_ports;
    float* control_values;
    int* audio_input_ports;
    int* audio_output_ports;
    int n_audio_inputs;
    int n_audio_outputs;
    
    // Audio buffers
    float* input_buffer;
    float* output_buffer;
} LV2Host;

void generate_dummy_audio(float* buffer, size_t frames, float frequency) {
    static float phase = 0.0f;
    float phase_increment = 2.0f * M_PI * frequency / SAMPLE_RATE;
    
    for (size_t i = 0; i < frames; i++) {
        buffer[i] = 0.5f * sinf(phase);
        phase += phase_increment;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }
}

LV2Host* lv2_host_new() {
    LV2Host* host = calloc(1, sizeof(LV2Host));
    if (!host) return NULL;
    
    // Initialize LILV
    host->world = lilv_world_new();
    lilv_world_load_all(host->world);
    
    // Initialize URID map
    host->urid_map.uris = NULL;
    host->urid_map.n_uris = 0;
    
    // Setup features
    host->map.handle = &host->urid_map;
    host->map.map = urid_map;
    
    host->unmap.handle = &host->urid_map;
    host->unmap.unmap = urid_unmap;
    
    host->log.handle = NULL;
    host->log.printf = logger_printf;
    host->log.vprintf = NULL;
    
    host->map_feature.URI = LV2_URID__map;
    host->map_feature.data = &host->map;
    
    host->unmap_feature.URI = LV2_URID__unmap;
    host->unmap_feature.data = &host->unmap;
    
    host->log_feature.URI = LV2_LOG__log;
    host->log_feature.data = &host->log;
    
    host->features[0] = &host->map_feature;
    host->features[1] = &host->unmap_feature;
    host->features[2] = &host->log_feature;
    host->features[3] = NULL;
    
    // Allocate audio buffers
    host->input_buffer = calloc(BUFFER_SIZE, sizeof(float));
    host->output_buffer = calloc(BUFFER_SIZE, sizeof(float));
    
    return host;
}

void lv2_host_free(LV2Host* host) {
    if (!host) return;
    
    if (host->instance) {
        lilv_instance_deactivate(host->instance);
        lilv_instance_free(host->instance);
    }
    
    free(host->control_values);
    free(host->audio_input_ports);
    free(host->audio_output_ports);
    free(host->input_buffer);
    free(host->output_buffer);
    
    // Free URID map
    for (size_t i = 0; i < host->urid_map.n_uris; i++) {
        free(host->urid_map.uris[i]);
    }
    free(host->urid_map.uris);
    
    if (host->world) {
        lilv_world_free(host->world);
    }
    
    free(host);
}

int lv2_host_load_plugin(LV2Host* host, const char* plugin_uri) {
    if (!host || !plugin_uri) return -1;
    
    printf("Loading plugin: %s\n", plugin_uri);
    
    // Find plugin
    LilvNode* uri = lilv_new_uri(host->world, plugin_uri);
    if (!uri) {
        printf("Error: Invalid plugin URI\n");
        return -1;
    }
    
    const LilvPlugins* plugins = lilv_world_get_all_plugins(host->world);
    host->plugin = lilv_plugins_get_by_uri(plugins, uri);
    lilv_node_free(uri);
    
    if (!host->plugin) {
        printf("Error: Plugin not found\n");
        return -1;
    }
    
    // Get plugin information
    LilvNode* name_node = lilv_plugin_get_name(host->plugin);
    printf("Plugin name: %s\n", lilv_node_as_string(name_node));
    lilv_node_free(name_node);
    
    // Analyze ports
    host->n_ports = lilv_plugin_get_num_ports(host->plugin);
    host->control_values = calloc(host->n_ports, sizeof(float));
    
    // Get port ranges
    float* min_values = calloc(host->n_ports, sizeof(float));
    float* max_values = calloc(host->n_ports, sizeof(float));
    float* def_values = calloc(host->n_ports, sizeof(float));
    lilv_plugin_get_port_ranges_float(host->plugin, min_values, max_values, def_values);
    
    // Create port type nodes
    LilvNode* audio_class = lilv_new_uri(host->world, LV2_CORE__AudioPort);
    LilvNode* control_class = lilv_new_uri(host->world, LV2_CORE__ControlPort);
    LilvNode* input_class = lilv_new_uri(host->world, LV2_CORE__InputPort);
    LilvNode* output_class = lilv_new_uri(host->world, LV2_CORE__OutputPort);
    
    // Count audio ports
    host->n_audio_inputs = 0;
    host->n_audio_outputs = 0;
    
    for (uint32_t i = 0; i < host->n_ports; i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(host->plugin, i);
        
        if (lilv_port_is_a(host->plugin, port, audio_class)) {
            if (lilv_port_is_a(host->plugin, port, input_class)) {
                host->n_audio_inputs++;
            } else if (lilv_port_is_a(host->plugin, port, output_class)) {
                host->n_audio_outputs++;
            }
        }
    }
    
    // Allocate port arrays
    host->audio_input_ports = calloc(host->n_audio_inputs, sizeof(int));
    host->audio_output_ports = calloc(host->n_audio_outputs, sizeof(int));
    
    int input_idx = 0, output_idx = 0;
    
    // Categorize ports and set defaults
    for (uint32_t i = 0; i < host->n_ports; i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(host->plugin, i);
        LilvNode* port_name = lilv_port_get_name(host->plugin, port);
        
        printf("Port %d: %s ", i, lilv_node_as_string(port_name));
        
        if (lilv_port_is_a(host->plugin, port, audio_class)) {
            if (lilv_port_is_a(host->plugin, port, input_class)) {
                printf("(Audio Input)\n");
                host->audio_input_ports[input_idx++] = i;
            } else if (lilv_port_is_a(host->plugin, port, output_class)) {
                printf("(Audio Output)\n");
                host->audio_output_ports[output_idx++] = i;
            }
        } else if (lilv_port_is_a(host->plugin, port, control_class)) {
            // Set default values for control ports
            host->control_values[i] = def_values[i];
            if (lilv_port_is_a(host->plugin, port, input_class)) {
                printf("(Control Input) default: %.2f, range: %.2f - %.2f\n", 
                       def_values[i], min_values[i], max_values[i]);
            } else {
                printf("(Control Output)\n");
            }
        }
        
        lilv_node_free(port_name);
    }
    
    // Instantiate plugin
    host->instance = lilv_plugin_instantiate(host->plugin, SAMPLE_RATE, host->features);
    if (!host->instance) {
        printf("Error: Failed to instantiate plugin\n");
        goto cleanup;
    }
    
    printf("Plugin instantiated successfully\n");
    printf("Audio inputs: %d, Audio outputs: %d\n", host->n_audio_inputs, host->n_audio_outputs);
    
    // Connect ports
    for (uint32_t i = 0; i < host->n_ports; i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(host->plugin, i);
        
        if (lilv_port_is_a(host->plugin, port, audio_class)) {
            if (lilv_port_is_a(host->plugin, port, input_class)) {
                lilv_instance_connect_port(host->instance, i, host->input_buffer);
            } else if (lilv_port_is_a(host->plugin, port, output_class)) {
                lilv_instance_connect_port(host->instance, i, host->output_buffer);
            }
        } else if (lilv_port_is_a(host->plugin, port, control_class)) {
            lilv_instance_connect_port(host->instance, i, &host->control_values[i]);
        }
    }
    
    // Activate plugin
    lilv_instance_activate(host->instance);
    printf("Plugin activated\n");
    
    // Cleanup
cleanup:
    free(min_values);
    free(max_values);
    free(def_values);
    lilv_node_free(audio_class);
    lilv_node_free(control_class);
    lilv_node_free(input_class);
    lilv_node_free(output_class);
    
    return host->instance ? 0 : -1;
}

void lv2_host_set_control(LV2Host* host, uint32_t port, float value) {
    if (!host || port >= host->n_ports) return;
    
    host->control_values[port] = value;
    printf("Set control port %d to %.2f\n", port, value);
}

void lv2_host_process(LV2Host* host) {
    if (!host || !host->instance) return;
    
    // Generate dummy input (sine wave at 440 Hz)
    generate_dummy_audio(host->input_buffer, BUFFER_SIZE, 440.0f);
    
    // Clear output buffer
    memset(host->output_buffer, 0, BUFFER_SIZE * sizeof(float));
    
    // Process audio
    lilv_instance_run(host->instance, BUFFER_SIZE);
    
    // Calculate RMS levels for monitoring
    float input_rms = 0.0f, output_rms = 0.0f;
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        input_rms += host->input_buffer[i] * host->input_buffer[i];
        output_rms += host->output_buffer[i] * host->output_buffer[i];
    }
    input_rms = sqrtf(input_rms / BUFFER_SIZE);
    output_rms = sqrtf(output_rms / BUFFER_SIZE);
    
    printf("Processed %d samples - Input RMS: %.4f, Output RMS: %.4f\n", 
           BUFFER_SIZE, input_rms, output_rms);
}

void list_available_plugins(LV2Host* host) {
    if (!host) return;
    
    printf("\n=== Available LV2 Plugins ===\n");
    const LilvPlugins* plugins = lilv_world_get_all_plugins(host->world);
    
    LILV_FOREACH(plugins, i, plugins) {
        const LilvPlugin* plugin = lilv_plugins_get(plugins, i);
        LilvNode* name = lilv_plugin_get_name(plugin);
        LilvNode* uri = lilv_plugin_get_uri(plugin);
        
        printf("URI: %s\n", lilv_node_as_string(uri));
        printf("Name: %s\n", lilv_node_as_string(name));
        
        // Get plugin class
        const LilvPluginClass* pclass = lilv_plugin_get_class(plugin);
        LilvNode* class_label = lilv_plugin_class_get_label(pclass);
        printf("Class: %s\n", lilv_node_as_string(class_label));
        
        printf("---\n");
        
        lilv_node_free(name);
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    printf("LV2 Host with LILV - Dummy Audio Processing Demo\n");
    printf("================================================\n");
    
    // Create host
    LV2Host* host = lv2_host_new();
    if (!host) {
        printf("Error: Failed to create LV2 host\n");
        return 1;
    }
    
    // List available plugins
    list_available_plugins(host);
    
    // Default plugin URI (you can change this)
    const char* plugin_uri = NULL;
    
    if (argc > 1) {
        plugin_uri = argv[1];
    } else {
        // Try to find a simple plugin (delay, reverb, etc.)
        const LilvPlugins* plugins = lilv_world_get_all_plugins(host->world);
        LILV_FOREACH(plugins, i, plugins) {
            const LilvPlugin* plugin = lilv_plugins_get(plugins, i);
            LilvNode* uri = lilv_plugin_get_uri(plugin);
            const char* uri_str = lilv_node_as_string(uri);
            
            // Look for a simple effect plugin
            if (strstr(uri_str, "delay") || strstr(uri_str, "Delay") ||
                strstr(uri_str, "reverb") || strstr(uri_str, "Reverb") ||
                strstr(uri_str, "filter") || strstr(uri_str, "Filter")) {
                plugin_uri = uri_str;
                break;
            }
        }
    }
    
    if (!plugin_uri) {
        printf("No plugin specified. Usage: %s <plugin_uri>\n", argv[0]);
        printf("Or let the program auto-select a plugin from the list above.\n");
        lv2_host_free(host);
        return 1;
    }
    
    // Load plugin
    if (lv2_host_load_plugin(host, plugin_uri) != 0) {
        printf("Error: Failed to load plugin\n");
        lv2_host_free(host);
        return 1;
    }
    
    printf("\n=== Processing Audio ===\n");
    
    // Process several frames of dummy audio
    for (int frame = 0; frame < NUM_FRAMES / BUFFER_SIZE; frame++) {
        printf("Frame %d: ", frame + 1);
        lv2_host_process(host);
        
        // Optionally modify control parameters during processing
        if (frame == 1 && host->n_ports > 2) {
            // Try to find and modify a control parameter
            for (uint32_t i = 2; i < host->n_ports && i < 10; i++) {
                const LilvPort* port = lilv_plugin_get_port_by_index(host->plugin, i);
                LilvNode* control_class = lilv_new_uri(host->world, LV2_CORE__ControlPort);
                LilvNode* input_class = lilv_new_uri(host->world, LV2_CORE__InputPort);
                
                if (lilv_port_is_a(host->plugin, port, control_class) &&
                    lilv_port_is_a(host->plugin, port, input_class)) {
                    float new_value = host->control_values[i] * 0.5f; // Reduce by half
                    lv2_host_set_control(host, i, new_value);
                    break;
                }
                
                lilv_node_free(control_class);
                lilv_node_free(input_class);
            }
        }
    }
    
    printf("\n=== Demo Complete ===\n");
    printf("Successfully processed %d frames of dummy audio data\n", NUM_FRAMES);
    
    // Cleanup
    lv2_host_free(host);
    return 0;
}