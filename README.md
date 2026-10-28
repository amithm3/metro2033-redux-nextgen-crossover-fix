# Metro 2033 Redux (next-gen build): CrossOver / D3DMetal launch crash fix

A tiny `dinput8.dll` proxy that fixes the next-gen build of Metro 2033 Redux crashing a couple of seconds after launch in CrossOver with D3DMetal:

```
Unhandled page fault on read access to 0000000000000000 at address 0000000140831E55
```

**Download:** [`dinput8.dll` from the latest release](https://github.com/amithm3/metro2033-redux-nextgen-crossover-fix/releases/latest)

## Cause

The next-gen build renders in DirectX 11 but runs DLSS/FSR on a separate DirectX 12 device, joined by a DX11→DX12 bridge. That bridge needs D3D11 extended resource sharing, and D3DMetal doesn't support it, so the bridge fails. The game correctly marks DLSS as unavailable, but then runs FSR setup anyway, calls into a DX12 device that was never created, and crashes on a null pointer.

The FSR setup function only checks its own run-once flag. The game's bridge-failed flag sits two bytes after it in memory. Changing the compare's opcode from `0x80` to `0x83` widens it from one byte to four (`cmp byte` → `cmp dword`), so it also sees the bridge-failed flag and skips FSR setup. Both versions of the instruction are 7 bytes long, so nothing else moves.

This is the same bug and the same fix as the one found for Metro Last Light Redux. In 2033 Redux the instruction is at RVA `0x831E1F`:

```
80 3D F5 31 7A 00 00 48 8B D9 0F 85    ->    83 3D F5 31 7A 00 00 48 8B D9 0F 85
```

## Why a DLL instead of a hex edit

2033 Redux's `metro.exe` is wrapped in Steam's DRM (SteamStub), and all of its code is encrypted on disk. A hex editor can't find the bytes, and editing the encrypted bytes would just corrupt the exe.

The DLL does the same one-byte change in memory, after the game has decrypted itself:

- It forwards `DirectInput8Create`, the only function the game imports from dinput8, to Wine's real `dinput8.dll`.
- On load, it starts a thread that waits until all 12 expected bytes are present, then flips `80` to `83`. If the bytes never match, it changes nothing.
- It writes `metro2033_fsr_fix.log` next to `metro.exe` on each launch.

The exe on disk and the DRM are untouched, so Steam file verification won't remove it.

## Install

1. Quit the game.
2. Download `dinput8.dll` from the [latest release](https://github.com/amithm3/metro2033-redux-nextgen-crossover-fix/releases/latest) and copy it into the game folder, next to `metro.exe`.
3. In CrossOver, select your Steam bottle, open **Wine Configuration**, go to the **Libraries** tab, add `dinput8`, and set it to **Native then Builtin**.
4. Launch the game from Steam.
5. Check `metro2033_fsr_fix.log` in the game folder. It should say `patched after N ms`.

To uninstall, delete `dinput8.dll` and remove the `dinput8` override.

## In-game settings

FSR setup is skipped, and DLSS and MetalFX aren't available. Use **TSR** for upscaling or anti-aliasing.

## Compatibility

Built for the `metro.exe` with this SHA-256 (check with `shasum -a 256 metro.exe`):

```
7c35095f27beab83b31914029eca0da07991bba8630ec12ba97eec43ffc60c38
```

If a game update changes the exe, the log will say `expected bytes never appeared (different build?) - not patching`, and the crash will come back. The new offset then needs to be found and the DLL rebuilt. Please open an issue if that happens.

`dinput8.dll` SHA-256:

```
b4be51d011b245aca8969e4d55eb42bf05055682f740176f8aed82cd7816f27c
```

Tested on an M4 Pro with 24 GB, CrossOver 26.3, and the D3DMetal backend.

## Build from source

`dinput8.c` is the full source, about 80 lines. Please read it before running a DLL from the internet.

```sh
brew install mingw-w64
x86_64-w64-mingw32-gcc -O2 -shared -s -static-libgcc -o dinput8.dll dinput8.c -Wl,--kill-at
```

## Credits

The root-cause analysis and the `0x80` → `0x83` fix come from the Metro Last Light Redux CrossOver writeup. This project ports that fix to 2033 Redux and applies it at runtime because of SteamStub.

## License

[MIT](LICENSE)
