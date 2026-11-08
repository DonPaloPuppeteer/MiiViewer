# MiiViewer

**MiiVisor** is an experimental homebrew application for Nintendo Wii that renders Mii characters using a custom, independently developed rendering library.

The project explores Mii rendering on real Wii hardware, with the goal of implementing character rendering without relying on Nintendo's original Mii rendering libraries.

## About

MiiVisor is a homebrew development project focused on understanding and recreating the rendering of Mii characters.

Its rendering system is built around a custom library called `rfl.h`, developed independently for this project.

The application uses Mii data and external resource files provided by the user rather than distributing Nintendo-owned resources.

## Features

- Mii character rendering on Nintendo Wii.
- Custom, independently developed rendering library (`rfl.h`).
- Support for loading Mii data.
- Experimental 3D rendering using GX.
- Designed to run on real Wii hardware.

This is an experimental project, so functionality and compatibility may change as development continues.

## AI-Assisted Development

MiiVisor was developed with the assistance of artificial intelligence tools.

AI was used as a development assistant for tasks such as:

- Code generation and implementation assistance.
- Debugging and troubleshooting.
- Code analysis and reverse engineering research.
- Understanding Wii graphics and rendering concepts.
- Improving and refining the custom rendering system.

The project was directed, tested, integrated and developed through human work, with AI serving as a programming and research assistant.

AI assistance does not mean the project is an official Nintendo product or that it uses Nintendo's original source code.

## Requirements

To build the project, you will need:

- A Nintendo Wii console or a compatible Wii emulator.
- [devkitPro](https://devkitpro.org/).
- devkitPPC.
- libogc.
- A compatible SD card or USB storage device.

## Building

Make sure devkitPro and the required Wii development libraries are installed.

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/MiiViewer.git
```

Enter the project directory:

```bash
cd MiiViewer
```

Build the application:

```bash
make
```

If compilation succeeds, the generated homebrew executable will be `boot.dol`.

## External Resources

MiiVisor does not distribute Nintendo-owned Mii resource files.

Some functionality requires external resources, such as:

- `RFL_Res.dat`
- `RFL_DB.dat`

These files are not included in this repository and must be provided separately by the user from their own compatible environment.

MiiVisor does not claim ownership of these resources.

## Disclaimer

MiiVisor is an unofficial, experimental homebrew project.

- It is not affiliated with, endorsed by, or supported by Nintendo.
- Nintendo, Wii and Mii are trademarks or properties of their respective owners.
- This project is independently developed and is not intended to replace Nintendo's original software.
- No proprietary Nintendo resource files are distributed with this project.

## Project Status

**Experimental / Work in Progress**

Mii rendering and the custom rendering library are under active development. Features, compatibility and implementation details may change.

## Credits

- **Project development:** Palo
- **AI assistance:** Used for programming assistance, debugging, research and code analysis.
- **Nintendo:** Original Mii system and Wii hardware platform.

---

*Made as an independent homebrew development experiment for Nintendo Wii.*
