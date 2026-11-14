# Neptunium - reverse reverb VST3 for FL Studio

Drop a vocal line into the window. Neptunium reverses it, adds reverb and reverses it back,
so a swell builds up right before the first word. The result is written as a WAV into
Documents/Neptunium and can be dragged from the plugin straight onto the FL Studio playlist.

## Build without installing anything (GitHub Actions)
1. Create a free account on github.com and a new empty repository.
2. Upload ALL files of this folder (including the hidden folder `.github`) to it.
3. Open the Actions tab -> "Build Neptunium" -> Run workflow (it also starts after every upload).
4. After ~10 minutes open the finished run and download the artifact `Neptunium-Windows`.

## Install
Copy the folder `Neptunium.vst3` to `C:\Program Files\Common Files\VST3\`.
In FL Studio: Options -> Manage plugins -> Find installed plugins.
Insert Neptunium on a mixer track (it is an effect).

## Build locally
Install Visual Studio 2022 (Desktop C++) and CMake, then:
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

## License note
JUCE is dual licensed (AGPLv3 / commercial). Fine for personal use; check the JUCE license before selling the plugin.
