# LV2 Host Examples with LILV

This directory contains two LV2 host implementations using LILV to demonstrate loading and running LV2 plugins with dummy audio data.

## Files

- `lilv_full_test.c` - Simple LV2 host that auto-selects a plugin and processes dummy audio
- `lilv_host.c` - More comprehensive LV2 host with full plugin listing and URI selection
- Both programs demonstrate proper LILV usage for LV2 plugin hosting

## Building

Use the provided Makefile targets:

```bash
# Build simple host
make lilv-full-test

# Build comprehensive host  
make lilv-host
```

## Usage

### Simple Host (lilv_full_test)

```bash
./lilv_full_test
```

This will:
1. Load all available LV2 plugins
2. Auto-select a suitable audio effect plugin (delay, reverb, filter, etc.)
3. Instantiate and configure the plugin
4. Process 5 frames of dummy sine wave audio at different frequencies
5. Display input/output RMS values and processing status

### Comprehensive Host (lilv_host)

```bash
# Auto-select plugin
./lilv_host

# Or specify a plugin URI
./lilv_host "http://calf.sourceforge.net/plugins/CompensationDelay"
```

This will:
1. List all available plugins with their URIs and classes
2. Load the specified plugin (or auto-select if none specified)
3. Display detailed port information
4. Process dummy audio data with parameter changes
5. Show comprehensive processing statistics

## Features Demonstrated

### Core LILV Functionality
- World creation and plugin loading
- Plugin discovery and URI-based selection
- Plugin instantiation with features
- Port analysis and connection
- Audio processing with `lilv_instance_run()`

### LV2 Features
- Basic URID mapping for plugin compatibility
- **LV2 Options support** for buffer size, sample rate, and sequence size
- **Atom:Path support** for file path communication with plugins
- Atom sequence handling for plugin communication
- Patch message support for parameter changes
- Options get/set callbacks for plugin parameter queries
- Logging support for plugin debugging
- Control port management with default values
- Audio port connection for stereo processing
- Atom port connection for file path messaging
- Proper plugin activation/deactivation

### Audio Processing
- Dummy sine wave generation at various frequencies
- RMS level monitoring for input/output analysis
- Multi-frame processing with parameter changes
- Buffer management for real-time-like processing

## Sample Output

```
=== LV2 Host using LILV - Simple Demo ===

Loading all plugins...
Initialized atom sequences for file path communication
Initialized LV2 features:
  Buffer size: 512 samples
  Sample rate: 48000 Hz
  Sequence size: 8192 bytes
  Atom:Path support enabled
Found 866 plugins

Selected plugin: Calf Compensation Delay Line

Testing atom:Path support...
Sent file path via atom:Path: /tmp/test_audio.wav
Sent file path via atom:Path: /home/user/samples/kick.wav

Processing dummy audio data...
  Frame 1 (440 Hz): Input RMS=0.3513, Output RMS=0.3514 [PROCESSING]

LV2 Features Support:
  ✓ Buffer size options (min/max/nominal)
  ✓ Sample rate option  
  ✓ Sequence size option
  ✓ Options get/set callbacks implemented
  ✓ Atom:Path support for file loading
  ✓ Atom sequences for plugin communication
  ✓ Patch messages for parameter changes
```

## Code Structure

### Key Components

1. **URID Mapping**: Simple implementation for plugin compatibility
2. **LV2 Options**: Complete options interface with buffer size, sample rate, and sequence size support
3. **Atom:Path Support**: File path communication using atom sequences and patch messages
4. **Feature Setup**: Comprehensive LV2 features (URID map, options, logging, atom forge)
5. **Plugin Discovery**: Automatic plugin selection with filtering
6. **Port Analysis**: Categorization of audio/control/atom/input/output ports
7. **Audio Generation**: Sine wave generation for testing
8. **Processing Loop**: Real-time-like audio processing simulation with atom handling
9. **Options Testing**: Validation of plugin options interface support
10. **Atom Processing**: Handling of atom sequences for file paths and plugin communication

### Error Handling

Both hosts include proper error handling for:
- Plugin loading failures
- Instantiation problems  
- Port connection issues
- Memory allocation errors

## Integration Notes

These examples can be integrated into larger audio applications by:
- Replacing dummy audio generation with real audio input
- Adding GUI controls for parameter adjustment
- Implementing preset loading/saving
- Adding MIDI support for instrument plugins
- Implementing proper real-time audio threading

The code demonstrates proper LILV usage patterns that can be adapted for production audio applications.