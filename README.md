# amprack-6-pc
  
**Amprack 6 for PC** is a guitar effects processor for PC, designed to provide a flexible and powerful amp rack experience. This project is a major rewrite, focusing on improved audio processing and a modern plugin architecture.

## Features

- Modular guitar effects rack for PC
- Major rewrite of audio processing code for better performance and sound quality
- Uses the standard LV2 plugin loading mechanism for compatibility and extensibility
- Supports a wide range of LV2 effects and amp simulators
- User-friendly interface for configuring and chaining effects

## Getting Started

1. **Install LV2 plugins**  
   Make sure you have LV2 plugins installed on your system.

2. **Build the project**  
   ```sh
   git clone https://github.com/yourusername/amprack-6-pc.git
   cd amprack-6-pc
   mkdir build && cd build
   cmake ..
   make
   ```

3. **Run amprack-6-pc**  
   ```sh
   ./amprack-6-pc
   ```

## Plugin Support

amprack-6-pc loads standard LV2 plugins, allowing you to use a wide variety of open-source and commercial effects.

## Contributing

Contributions are welcome! Please open issues or pull requests on GitHub.

## License

This project is licensed under the MIT License.
  
## About the Original amprack Project

The original [amprack](https://amprack.in) was a Linux-based guitar effects processor focused on providing a simple, low-latency effects chain for live performance and recording. It featured:

- Real-time audio processing with JACK support
- A selection of built-in effects and amp simulations
- A straightforward, minimal user interface
- Open-source codebase for community contributions

**amprack-6-pc** builds upon the concepts of the original amprack, introducing a modernized architecture, improved plugin support, and enhanced audio quality for PC users.