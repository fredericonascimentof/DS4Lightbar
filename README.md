# DS4 Lightbar

DS4 Lightbar is a Windows desktop app for controlling the DualShock 4 lightbar. Build 1.0 focuses on the core workflow: detect the controller, choose how the light should behave, adjust brightness and transition speed, and keep the app running quietly from the system tray.

The app uses Qt/QML for the desktop interface and SDL3/HID access for controller detection and LED control.

This project is not affiliated with Sony, PlayStation, or DualShock.

## Build 1.0 Features

### Controller Detection

The app tries to detect a DualShock 4 automatically when it starts and keeps polling for connection changes while it is running.

Detection paths used in build 1.0:

- SDL3 Gamepad API
- SDL3 Joystick fallback
- SDL3 HIDAPI fallback
- Native Windows HID fallback

This layered approach exists because Windows can expose a DualShock 4 differently depending on Bluetooth drivers, USB mode, controller revision, and installed input tools.

The header status shows:

- controller connection state
- controller name when available
- battery percentage when the backend exposes it
- `--%` when battery data is unavailable

Battery percentage is not guaranteed in every fallback path. Some HID paths allow LED control but do not expose power information.

### Light Modes

Build 1.0 has three light behavior modes.

#### Static Color

Keeps the lightbar on a selected color.

Available controls:

- chromatic hue bar for choosing the color
- LED intensity slider
- `Keep light static` checkbox

When `Keep light static` is enabled, the light does not pulse. The transition speed control is locked because it has no effect in this state.

When `Keep light static` is disabled, the selected static color can pulse smoothly. In that case, transition speed controls how often the animation updates.

#### Spectrum Cycle

Cycles through the internal color palette with smooth transitions.

The current cycle is based on these colors:

- red
- cyan
- violet
- magenta
- green
- orange

Available controls:

- LED intensity
- transition speed

The cycle uses smooth color interpolation instead of blinking or fading to black between colors.

#### Lightbar Off

Turns the lightbar off by sending black (`0, 0, 0`) to the controller.

In this mode:

- LED intensity is locked
- transition speed is locked
- animation is stopped

### UI

Build 1.0 includes a compact desktop window with:

- controller status pill
- live lightbar preview
- light behavior dropdown
- intensity control
- transition speed control
- chromatic color selector for static mode
- runtime log window

The main window can be closed without stopping the app. Closing the window hides it and keeps the app running in the system tray.

### System Tray

The app stays available from the Windows system tray.

Tray actions:

- open the main window
- open the runtime log
- exit the app

Clicking or double-clicking the tray icon opens the main window.

### Runtime Log

The log window shows what the app is doing while it runs.

Examples of logged events:

- SDL initialization
- controller scans
- controller opened or removed
- selected light mode
- intensity changes
- transition speed changes
- lightbar color transitions
- HID fallback attempts
- LED send failures

The log is useful for debugging cases where Windows sees the controller but SDL/HID access does not expose LED control.

### Single Instance

Build 1.0 prevents multiple app instances from running at the same time on Windows. If the app is already running, starting it again exits immediately instead of opening a second copy.

## Requirements

Tested target:

- Windows 11
- DualShock 4 controller
- Qt 6.x with MinGW
- CMake 3.20+
- Ninja or MinGW Makefiles
- vcpkg
- SDL3

The project currently targets Windows. Some of the fallback detection code uses native Windows HID APIs.

## Building

Install the required tools first:

- Qt 6 with MinGW
- CMake
- Ninja
- vcpkg

Install dependencies through vcpkg:

```bash
vcpkg install sdl3:x64-mingw-dynamic
```

The repository includes `CMakePresets.json`, but the paths inside it may need to be adjusted for your machine. In particular, check:

- `CMAKE_PREFIX_PATH`
- `CMAKE_TOOLCHAIN_FILE`
- `CMAKE_MAKE_PROGRAM`
- `CMAKE_C_COMPILER`
- `CMAKE_CXX_COMPILER`

Configure and build:

```bash
cmake --preset qt-mingw-debug
cmake --build --preset qt-mingw-debug
```

Run the debug build:

```bash
build/qt-mingw-debug/DS4Lightbar.exe
```

## Qt Creator Workflow

Open this file in Qt Creator:

```text
CMakeLists.txt
```

Use the Qt/MinGW kit that matches your local Qt installation.

The included helper scripts are:

- `open_qt_creator.cmd`: opens the project in Qt Creator
- `build_debug.cmd`: builds the debug preset
- `run_debug.cmd`: runs the debug executable

In debug builds, QML live reload is enabled. Editing `qml/Main.qml` while the app is running reloads the interface without restarting the application.

## Project Structure

```text
assets/
  icon.png

diagnostics/
  sdl_probe.cpp

qml/
  Main.qml

CMakeLists.txt
CMakePresets.json
LightbarController.cpp
LightbarController.h
main.cpp
vcpkg.json
README.md
```

### Main Files

- `main.cpp`: application startup, single-instance guard, tray icon, QML loading
- `LightbarController.h`: properties and signals exposed to QML
- `LightbarController.cpp`: controller detection, animation logic, LED output, logs
- `qml/Main.qml`: desktop UI
- `assets/icon.png`: app icon
- `diagnostics/sdl_probe.cpp`: small SDL diagnostic probe for controller detection

## Recommended GitHub Folder

Use this folder as the project root:

```text
F:\DS4Lightbar-QML
```

For GitHub, copy or commit the contents of `F:\DS4Lightbar-QML`, not the parent folder.

Do not include generated build output such as:

- `build/`
- `.vs/`
- `vcpkg_installed/`
- `.qt/`
- `.rcc/`
- `*.exe`
- `*.dll`
- runtime logs

The `legacy-sdl/` folder is an archived copy of the older SDL-only implementation. For a clean open source release, it is better to leave it out of the main branch or move it to a separate branch/tag.

## Known Limitations

- Build 1.0 does not include an installer.
- Settings are applied live but are not persisted as profiles yet.
- Battery status depends on the input path exposed by Windows/SDL.
- Some Bluetooth drivers expose the controller as input-only and may not allow LED control.
- The UI is currently designed for a fixed compact desktop window.
- The app is Windows-focused because the HID fallback path uses WinAPI.

## Open Source Notes

Recommended files before publishing:

- `README.md`
- `.gitignore`
- `LICENSE`
- source files
- QML files
- app-owned assets only

Suggested license:

- MIT, if you want a permissive license
- GPL, if you want modifications to remain open source

Avoid using official PlayStation or Sony artwork unless you have the right license. Keep the project visually distinct and include the affiliation disclaimer.
