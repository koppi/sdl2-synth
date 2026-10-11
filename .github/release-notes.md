Binary builds of the SDL2 synth for Linux, macOS and Windows.

## Downloads

| Package | Contents |
|---------|----------|
| `sdl2-synth-vst3-<version>-<platform>` | **VST3 instrument plug-in**: copy `sdl2-synth.vst3` into your VST3 folder (see `INSTALL.txt` inside) |
| `sdl2-synth-standalone-<version>-<platform>` | Standalone application (single executable) |

Platforms: `linux-x86_64`, `macos-universal` (Apple Silicon + Intel, macOS 11+), `windows-x86_64`.
`SHA256SUMS.txt` lists the checksum of every file.

macOS builds are ad-hoc signed unless the release was produced with an Apple
Developer ID. If macOS blocks a download, run
`xattr -dr com.apple.quarantine <file>` once (see `INSTALL.txt`).

VST is a registered trademark of Steinberg Media Technologies GmbH.
