#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <lilv/lilv.h>
#include <lv2/core/lv2.h>
#include <lv2/options/options.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/patch/patch.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 512

// Simple URID map for basic functionality
static char* uris[1000];
static size_t n_uris = 0;

// Options and Atom support
typedef struct {
    LV2_URID atom_Int;
    LV2_URID atom_Float;
    LV2_URID atom_Path;
    LV2_URID atom_String;
    LV2_URID atom_Sequence;
    LV2_URID atom_eventTransfer;
    LV2_URID patch_Set;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID bufsz_minBlockLength;
    LV2_URID bufsz_maxBlockLength;
    LV2_URID bufsz_nominalBlockLength;
    LV2_URID bufsz_sequenceSize;
    LV2_URID param_sampleRate;
} OptionsURIDs;

static LV2_URID simple_map(LV2_URID_Map_Handle handle, const char* uri) {
    for (size_t i = 0; i < n_uris; i++) {
        if (strcmp(uris[i], uri) == 0) {
            return i + 1;
        }
    }
    if (n_uris < 1000) {
        uris[n_uris] = strdup(uri);
        return ++n_uris;
    }
    return 0;
}

// Global options and atom state
static OptionsURIDs options_urids;
static int32_t block_length = BUFFER_SIZE;
static float sample_rate = SAMPLE_RATE;
static int32_t sequence_size = 8192;

// Atom forge for creating atom messages
static LV2_Atom_Forge forge;
static uint8_t forge_buffer[8192];
static LV2_Atom_Sequence* input_sequence = NULL;
static LV2_Atom_Sequence* output_sequence = NULL;

// Function prototypes
static int pass_filename_to_plugin(const char* filename);

/*
 * Usage example:
 *   pass_filename_to_plugin("/path/to/sample.wav");
 *   pass_filename_to_plugin("/home/user/audio/kick.flac");
 *   pass_filename_to_plugin("./relative/path/snare.aiff");
 */

// Options get callback
static uint32_t options_get(LV2_Handle instance,
                           LV2_Options_Option* options) {
    (void)instance;  // Unused parameter
    for (LV2_Options_Option* opt = options; opt->key || opt->value; ++opt) {
        if (opt->key == options_urids.bufsz_minBlockLength ||
            opt->key == options_urids.bufsz_maxBlockLength ||
            opt->key == options_urids.bufsz_nominalBlockLength) {
            if (opt->size >= sizeof(int32_t)) {
                *(int32_t*)opt->value = block_length;
                opt->type = options_urids.atom_Int;
                opt->size = sizeof(int32_t);
            }
        } else if (opt->key == options_urids.bufsz_sequenceSize) {
            if (opt->size >= sizeof(int32_t)) {
                *(int32_t*)opt->value = sequence_size;
                opt->type = options_urids.atom_Int;
                opt->size = sizeof(int32_t);
            }
        } else if (opt->key == options_urids.param_sampleRate) {
            if (opt->size >= sizeof(float)) {
                *(float*)opt->value = sample_rate;
                opt->type = options_urids.atom_Float;
                opt->size = sizeof(float);
            }
        } else {
            opt->type = 0;  // Unsupported option
            opt->size = 0;
        }
    }
    return LV2_OPTIONS_SUCCESS;
}

// Options set callback  
static uint32_t options_set(LV2_Handle instance,
                           const LV2_Options_Option* options) {
    (void)instance;  // Unused parameter
    for (const LV2_Options_Option* opt = options; opt->key || opt->value; ++opt) {
        if (opt->key == options_urids.bufsz_minBlockLength ||
            opt->key == options_urids.bufsz_maxBlockLength ||
            opt->key == options_urids.bufsz_nominalBlockLength) {
            if (opt->type == options_urids.atom_Int && opt->size == sizeof(int32_t)) {
                block_length = *(const int32_t*)opt->value;
                printf("[Options] Set block length to %d\n", block_length);
            }
        } else if (opt->key == options_urids.bufsz_sequenceSize) {
            if (opt->type == options_urids.atom_Int && opt->size == sizeof(int32_t)) {
                sequence_size = *(const int32_t*)opt->value;
                printf("[Options] Set sequence size to %d\n", sequence_size);
            }
        } else if (opt->key == options_urids.param_sampleRate) {
            if (opt->type == options_urids.atom_Float && opt->size == sizeof(float)) {
                sample_rate = *(const float*)opt->value;
                printf("[Options] Set sample rate to %.0f\n", sample_rate);
            }
        }
    }
    return LV2_OPTIONS_SUCCESS;
}

// Create an atom:Path message
static LV2_Atom* create_path_atom(const char* path) {
    lv2_atom_forge_set_buffer(&forge, forge_buffer, sizeof(forge_buffer));
    
    LV2_Atom_Forge_Frame frame;
    LV2_Atom* atom = (LV2_Atom*)lv2_atom_forge_object(
        &forge, &frame, 0, options_urids.patch_Set);
    
    lv2_atom_forge_key(&forge, options_urids.patch_property);
    lv2_atom_forge_urid(&forge, options_urids.atom_Path);
    lv2_atom_forge_key(&forge, options_urids.patch_value);
    lv2_atom_forge_path(&forge, path, strlen(path));
    
    lv2_atom_forge_pop(&forge, &frame);
    
    return atom;
}

// Initialize atom sequence for plugin communication
static void init_atom_sequences() {
    const size_t seq_size = sizeof(LV2_Atom_Sequence) + 8192;
    
    input_sequence = (LV2_Atom_Sequence*)calloc(1, seq_size);
    output_sequence = (LV2_Atom_Sequence*)calloc(1, seq_size);
    
    input_sequence->atom.type = options_urids.atom_Sequence;
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    input_sequence->body.unit = options_urids.atom_eventTransfer;
    input_sequence->body.pad = 0;
    
    output_sequence->atom.type = options_urids.atom_Sequence;
    output_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    output_sequence->body.unit = options_urids.atom_eventTransfer;
    output_sequence->body.pad = 0;
    
    printf("Initialized atom sequences for file path communication\n");
}

// Send a file path to plugin via atom:Path
static void send_file_path(const char* file_path) {
    if (!input_sequence) return;
    
    // Reset sequence
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    
    // Create path atom
    LV2_Atom* path_atom = create_path_atom(file_path);
    if (path_atom) {
        // Add to sequence at frame 0
        lv2_atom_sequence_append_event(
            input_sequence, 8192,
            &(LV2_Atom_Event){
                .time.frames = 0,
                .body = *path_atom
            });
        
        printf("Sent file path via atom:Path: %s\n", file_path);
    }
}

// Pass a filename to the plugin with validation and error handling
static int pass_filename_to_plugin(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Invalid filename provided\n");
        return -1;
    }
    
    if (!input_sequence) {
        printf("Error: Atom sequences not initialized\n");
        return -1;
    }
    
    // Check if file exists (optional validation)
    FILE* test_file = fopen(filename, "r");
    if (test_file) {
        fclose(test_file);
        printf("File exists, passing to plugin: %s\n", filename);
    } else {
        printf("Warning: File may not exist, but passing to plugin anyway: %s\n", filename);
    }
    
    // Reset sequence for new file
    input_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
    
    // Create and send path atom
    lv2_atom_forge_set_buffer(&forge, forge_buffer, sizeof(forge_buffer));
    
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_sequence_head(&forge, &frame, options_urids.atom_eventTransfer);
    
    // Add path event at frame 0
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_object(&forge, &frame, 0, options_urids.patch_Set);
    lv2_atom_forge_key(&forge, options_urids.patch_property);
    lv2_atom_forge_urid(&forge, options_urids.atom_Path);
    lv2_atom_forge_key(&forge, options_urids.patch_value);
    lv2_atom_forge_path(&forge, filename, strlen(filename));
    
    lv2_atom_forge_pop(&forge, &frame);
    
    // Copy forged sequence to input sequence
    LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*)forge.buf;
    memcpy(input_sequence, seq, sizeof(LV2_Atom_Sequence) + seq->atom.size);
    
    printf("Successfully passed filename to plugin: %s\n", filename);
    printf("  Atom sequence size: %u bytes\n", input_sequence->atom.size);
    printf("  Event time: 0 frames\n");
    
    return 0;
}

// Process atom sequences for file paths
static void process_atom_sequences() {
    if (!output_sequence) return;
    
    LV2_ATOM_SEQUENCE_FOREACH(output_sequence, ev) {
        const LV2_Atom* atom = &ev->body;
        
        if (atom->type == options_urids.atom_Path) {
            const char* path = (const char*)(atom + 1);
            printf("Received atom:Path from plugin: %s\n", path);
        } else if (atom->type == options_urids.atom_String) {
            const char* str = (const char*)(atom + 1);
            printf("Received atom:String from plugin: %s\n", str);
        }
    }
    
    // Reset output sequence for next cycle
    output_sequence->atom.size = sizeof(LV2_Atom_Sequence_Body);
}

int main(int argc, char* argv[]) {
    printf("=== LV2 Host using LILV - Simple Demo ===\n\n");
    
    // Check for filename argument
    const char* user_filename = NULL;
    if (argc > 1) {
        user_filename = argv[1];
        printf("User provided filename: %s\n\n", user_filename);
    }
    
    // Initialize LILV world
    LilvWorld* world = lilv_world_new();
    if (!world) {
        printf("Failed to create LILV world\n");
        return 1;
    }
    
    printf("Loading all plugins...\n");
    lilv_world_load_all(world);
    
    // Setup basic features
    LV2_URID_Map map = { NULL, simple_map };
    LV2_Feature map_feature = { LV2_URID__map, &map };
    
    // Initialize options and atom URIDs
    options_urids.atom_Int = simple_map(NULL, LV2_ATOM__Int);
    options_urids.atom_Float = simple_map(NULL, LV2_ATOM__Float);
    options_urids.atom_Path = simple_map(NULL, LV2_ATOM__Path);
    options_urids.atom_String = simple_map(NULL, LV2_ATOM__String);
    options_urids.atom_Sequence = simple_map(NULL, LV2_ATOM__Sequence);
    options_urids.atom_eventTransfer = simple_map(NULL, LV2_ATOM__eventTransfer);
    options_urids.patch_Set = simple_map(NULL, LV2_PATCH__Set);
    options_urids.patch_property = simple_map(NULL, LV2_PATCH__property);
    options_urids.patch_value = simple_map(NULL, LV2_PATCH__value);
    options_urids.bufsz_minBlockLength = simple_map(NULL, LV2_BUF_SIZE__minBlockLength);
    options_urids.bufsz_maxBlockLength = simple_map(NULL, LV2_BUF_SIZE__maxBlockLength);
    options_urids.bufsz_nominalBlockLength = simple_map(NULL, LV2_BUF_SIZE__nominalBlockLength);
    options_urids.bufsz_sequenceSize = simple_map(NULL, LV2_BUF_SIZE__sequenceSize);
    options_urids.param_sampleRate = simple_map(NULL, "http://lv2plug.in/ns/lv2core#sampleRate");
    
    // Setup options feature
    LV2_Options_Interface options_iface = { options_get, options_set };
    LV2_Feature options_interface_feature = { LV2_OPTIONS__interface, &options_iface };
    
    // Create options array with current values
    static int32_t block_len_val = BUFFER_SIZE;
    static float sample_rate_val = SAMPLE_RATE;
    static int32_t seq_size_val = 8192;
    
    LV2_Options_Option options[] = {
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_minBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_maxBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_nominalBlockLength,
          sizeof(int32_t), options_urids.atom_Int, &block_len_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_sequenceSize,
          sizeof(int32_t), options_urids.atom_Int, &seq_size_val },
        { LV2_OPTIONS_INSTANCE, 0, options_urids.param_sampleRate,
          sizeof(float), options_urids.atom_Float, &sample_rate_val },
        { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL }  // Terminator
    };
    
    LV2_Feature options_array_feature = { LV2_OPTIONS__options, options };
    
        const LV2_Feature* features[] = { 
        &map_feature, 
        &options_interface_feature,
        &options_array_feature,
        NULL 
    };
    
    // Initialize atom forge and sequences
    lv2_atom_forge_init(&forge, &map);
    init_atom_sequences();
    
    printf("Initialized LV2 features:\n");
    printf("  Buffer size: %d samples\n", BUFFER_SIZE);
    printf("  Sample rate: %d Hz\n", SAMPLE_RATE);
    printf("  Sequence size: %d bytes\n", seq_size_val);
    printf("  Atom:Path support enabled\n");
    
    // Get all plugins
    const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
    printf("Found %d plugins\n\n", lilv_plugins_size(plugins));
    
    // Find a suitable plugin (preferably an effect)
    const LilvPlugin* chosen_plugin = NULL;
    const char* chosen_name = NULL;
    
    printf("Looking for a suitable audio effect plugin...\n");
    LILV_FOREACH(plugins, i, plugins) {
        const LilvPlugin* plugin = lilv_plugins_get(plugins, i);
        LilvNode* name = lilv_plugin_get_name(plugin);
        LilvNode* uri = lilv_plugin_get_uri(plugin);
        
        const char* name_str = lilv_node_as_string(name);
        const char* uri_str = lilv_node_as_string(uri);
        
        // Look for plugins that are likely to be audio effects
        if (strstr(uri_str, "delay") || strstr(uri_str, "Delay") ||
            strstr(uri_str, "reverb") || strstr(uri_str, "Reverb") ||
            strstr(uri_str, "filter") || strstr(uri_str, "Filter") ||
            strstr(uri_str, "distortion") || strstr(uri_str, "Distortion") ||
            strstr(name_str, "Delay") || strstr(name_str, "Reverb") ||
            strstr(name_str, "Filter") || strstr(name_str, "Distortion")) {
            
            chosen_plugin = plugin;
            chosen_name = name_str;
            printf("Selected plugin: %s\n", name_str);
            printf("URI: %s\n\n", uri_str);
            lilv_node_free(name);
            break;
        }
        
        lilv_node_free(name);
    }
    
    // If no effect found, use the first available plugin
    if (!chosen_plugin) {
        printf("No obvious effect plugin found, using first available plugin...\n");
        LILV_FOREACH(plugins, i, plugins) {
            chosen_plugin = lilv_plugins_get(plugins, i);
            LilvNode* name = lilv_plugin_get_name(chosen_plugin);
            chosen_name = lilv_node_as_string(name);
            printf("Using: %s\n\n", chosen_name);
            lilv_node_free(name);
            break;
        }
    }
    
    if (!chosen_plugin) {
        printf("No plugins available!\n");
        lilv_world_free(world);
        return 1;
    }
    
    // Analyze plugin ports
    uint32_t n_ports = lilv_plugin_get_num_ports(chosen_plugin);
    printf("Plugin has %d ports:\n", n_ports);
    
    // Get port ranges
    float* min_values = calloc(n_ports, sizeof(float));
    float* max_values = calloc(n_ports, sizeof(float));  
    float* def_values = calloc(n_ports, sizeof(float));
    lilv_plugin_get_port_ranges_float(chosen_plugin, min_values, max_values, def_values);
    
    // Create port type nodes
    LilvNode* audio_class = lilv_new_uri(world, LV2_CORE__AudioPort);
    LilvNode* control_class = lilv_new_uri(world, LV2_CORE__ControlPort);
    LilvNode* input_class = lilv_new_uri(world, LV2_CORE__InputPort);
    LilvNode* output_class = lilv_new_uri(world, LV2_CORE__OutputPort);
    
    // Find audio ports
    int audio_input = -1, audio_output = -1;
    float* control_values = calloc(n_ports, sizeof(float));
    
    for (uint32_t i = 0; i < n_ports; i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(chosen_plugin, i);
        LilvNode* port_name = lilv_port_get_name(chosen_plugin, port);
        
        printf("  Port %d: %s - ", i, lilv_node_as_string(port_name));
        
        if (lilv_port_is_a(chosen_plugin, port, audio_class)) {
            if (lilv_port_is_a(chosen_plugin, port, input_class)) {
                printf("Audio Input\n");
                if (audio_input == -1) audio_input = i;
            } else if (lilv_port_is_a(chosen_plugin, port, output_class)) {
                printf("Audio Output\n");
                if (audio_output == -1) audio_output = i;
            }
        } else if (lilv_port_is_a(chosen_plugin, port, control_class)) {
            control_values[i] = def_values[i];
            if (lilv_port_is_a(chosen_plugin, port, input_class)) {
                printf("Control Input (default: %.2f, range: %.2f - %.2f)\n", 
                       def_values[i], min_values[i], max_values[i]);
            } else {
                printf("Control Output\n");
            }
        } else {
            printf("Other\n");
        }
        
        lilv_node_free(port_name);
    }
    
    if (audio_input == -1 || audio_output == -1) {
        printf("\nWarning: Plugin doesn't have proper audio input/output ports\n");
        printf("Audio input port: %d, Audio output port: %d\n", audio_input, audio_output);
    }
    
    // Instantiate plugin
    printf("\nInstantiating plugin...\n");
    LilvInstance* instance = lilv_plugin_instantiate(chosen_plugin, SAMPLE_RATE, features);
    if (!instance) {
        printf("Failed to instantiate plugin!\n");
        goto cleanup;
    }
    
    // Create audio buffers
    float* input_buffer = calloc(BUFFER_SIZE, sizeof(float));
    float* output_buffer = calloc(BUFFER_SIZE, sizeof(float));
    
    // Connect ports
    printf("Connecting ports...\n");
    for (uint32_t i = 0; i < n_ports; i++) {
        const LilvPort* port = lilv_plugin_get_port_by_index(chosen_plugin, i);
        
        if (lilv_port_is_a(chosen_plugin, port, audio_class)) {
            if (i == audio_input) {
                lilv_instance_connect_port(instance, i, input_buffer);
                printf("  Connected audio input port %d\n", i);
            } else if (i == audio_output) {
                lilv_instance_connect_port(instance, i, output_buffer);
                printf("  Connected audio output port %d\n", i);
            }
        } else if (lilv_port_is_a(chosen_plugin, port, control_class)) {
            lilv_instance_connect_port(instance, i, &control_values[i]);
            printf("  Connected control port %d (value: %.2f)\n", i, control_values[i]);
        } else {
            // Check for atom ports (for file paths, etc.)
            LilvNode* atom_class = lilv_new_uri(world, LV2_ATOM__AtomPort);
            if (lilv_port_is_a(chosen_plugin, port, atom_class)) {
                if (lilv_port_is_a(chosen_plugin, port, input_class)) {
                    lilv_instance_connect_port(instance, i, input_sequence);
                    printf("  Connected atom input port %d (for file paths)\n", i);
                } else if (lilv_port_is_a(chosen_plugin, port, output_class)) {
                    lilv_instance_connect_port(instance, i, output_sequence);
                    printf("  Connected atom output port %d (for responses)\n", i);
                }
            }
            lilv_node_free(atom_class);
        }
    }
    
    // Activate plugin
    printf("\nActivating plugin...\n");
    lilv_instance_activate(instance);
    
    // Test options interface if plugin supports it
    const LV2_Options_Interface* plugin_options = NULL;
    const LV2_Descriptor* lv2_desc = lilv_instance_get_descriptor(instance);
    if (lv2_desc && lv2_desc->extension_data) {
        plugin_options = (const LV2_Options_Interface*)
            lv2_desc->extension_data(LV2_OPTIONS__interface);
    }
    
    if (plugin_options) {
        printf("Plugin supports options interface! Testing...\n");
        
        // Test getting buffer size option
        int32_t test_buffer_size = 0;
        LV2_Options_Option test_option = {
            LV2_OPTIONS_INSTANCE, 0, options_urids.bufsz_nominalBlockLength,
            sizeof(int32_t), 0, &test_buffer_size
        };
        
        if (plugin_options->get(lilv_instance_get_handle(instance), &test_option) == LV2_OPTIONS_SUCCESS) {
            printf("  Plugin buffer size option: %d samples\n", test_buffer_size);
        } else {
            printf("  Plugin doesn't support buffer size option\n");
        }
    } else {
        printf("Plugin doesn't support options interface\n");
    }
    
    // Test atom:Path functionality
    printf("\nTesting atom:Path support...\n");
    send_file_path("/tmp/test_audio.wav");
    send_file_path("/home/user/samples/kick.wav");
    
    // Test the dedicated filename passing function
    printf("\nTesting filename passing function...\n");
    pass_filename_to_plugin("/usr/share/sounds/alsa/Front_Left.wav");
    pass_filename_to_plugin("/tmp/nonexistent.wav");
    pass_filename_to_plugin("relative_path.wav");
    
    // Generate and process dummy audio
    printf("\nProcessing dummy audio data...\n");
    
    for (int frame = 0; frame < 5; frame++) {
        // Generate sine wave input (440 Hz)
        static float phase = 0.0f;
        float freq = 440.0f + frame * 110.0f; // Change frequency each frame
        float phase_inc = 2.0f * M_PI * freq / SAMPLE_RATE;
        
        for (int i = 0; i < BUFFER_SIZE; i++) {
            input_buffer[i] = 0.5f * sinf(phase);
            phase += phase_inc;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        
        // Clear output
        memset(output_buffer, 0, BUFFER_SIZE * sizeof(float));
        
        // Process audio and atoms
        lilv_instance_run(instance, BUFFER_SIZE);
        
        // Process any atom responses from plugin
        process_atom_sequences();
        
        // Calculate RMS levels
        float input_rms = 0.0f, output_rms = 0.0f;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            input_rms += input_buffer[i] * input_buffer[i];
            output_rms += output_buffer[i] * output_buffer[i];
        }
        input_rms = sqrtf(input_rms / BUFFER_SIZE);
        output_rms = sqrtf(output_rms / BUFFER_SIZE);
        
        printf("  Frame %d (%.0f Hz): Input RMS=%.4f, Output RMS=%.4f", 
               frame + 1, freq, input_rms, output_rms);
        
        if (output_rms > 0.001f) {
            printf(" [PROCESSING]");
        } else {
            printf(" [SILENT]");
        }
        printf("\n");
        
        // Show first few output samples for debugging
        if (frame == 0) {
            printf("    First 10 output samples: ");
            for (int i = 0; i < 10; i++) {
                printf("%.3f ", output_buffer[i]);
            }
            printf("\n");
        }
    }
    
    printf("\n=== Processing Complete ===\n");
