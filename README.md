# Unreal Offset Finder - DMA

A DMA-based tool for finding and tracking Unreal Engine global offsets for memory analysis and research purposes.

## Description

This tool uses Direct Memory Access (DMA) to scan Unreal Engine processes and locate critical global offsets such as GWorld, GNames, and other engine internals. It provides a GUI interface for real-time offset discovery and tracking.

## Features

- DMA-based memory scanning
- Real-time offset discovery
- ImGui-based user interface
- Offset export functionality
- Support for multiple Unreal Engine games

## Building

Open `UnrealOffsetFinder-DMA.sln` in Visual Studio and build the solution.

### Requirements

- Visual Studio 2019/2022
- Windows SDK
- DMA hardware support

## Usage

1. Ensure your DMA device is properly connected
2. Launch the target Unreal Engine game
3. Run UnrealOffsetFinder-DMA.exe
4. Select the target process
5. Initiate offset scanning

## Output

Found offsets are saved to `offsets.txt` in the executable directory. Example output format can be found in `example_offsets.txt`.

## Disclaimer

This tool is intended for educational and research purposes only. Use responsibly and in accordance with applicable laws and terms of service.