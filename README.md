# innoextract-nb

**Boost-free fork** of [innoextract](https://github.com/dscharrer/innoextract) for use with
[Akhenaten](https://github.com/dalerank/Akhenaten) — a Pharaoh / Cleopatra reimplementation.

Upstream innoextract unpacks [Inno Setup](https://jrsoftware.org/isinfo.php) installers
(including many GOG.com game installers) without running them under Wine.
This fork keeps that goal, but **removes the Boost dependency** so Akhenaten can ship
`innoextract` as a small external helper next to the game binary.

| | Upstream | This fork (`innoextract-nb`) |
|---|---|---|
| Boost | Required | **Not used** |
| Compression | via Boost.Iostreams + liblzma | zlib + bzip2 + liblzma |
| CLI / filesystem / time | Boost.ProgramOptions / Filesystem / DateTime | custom CLI + `std::filesystem` + `std::chrono` |
| Primary consumer | general unpack tool | Akhenaten installer bootstrap |

Supported installer range matches upstream intent: Inno Setup-based packages, including
GOG variants used for **Pharaoh Gold** and similar titles.

License: **ZLIB** (same as upstream) — see [LICENSE](LICENSE).

Upstream site: https://constexpr.org/innoextract/  
Upstream author: [Daniel Scharrer](https://constexpr.org/)

## Project goals

1. **No Boost** — build and link without Boost headers or libraries.
2. **Stay useful for Akhenaten** — extract GOG / Inno Setup Pharaoh (+ Cleopatra) installers
   into a folder the game can use as `data_directory` / `PharaohData`.
3. **Keep unpack fidelity** — same Inno Setup / GOG extract behaviour as upstream for the
   installer formats Akhenaten cares about.
4. **Remain a standalone tool** — never link into `akhenaten.exe`; ship as `innoextract.exe`
   beside the game (spawned at runtime).
5. **Stay close to upstream** — rebase / cherry-pick when practical; do not turn this into a
   general Windows installer suite (InstallShield demos belong to [unshield](https://github.com/twogood/unshield) + 7-Zip in Akhenaten).

Non-goals: GUI, component-selective install UI, running installer scripts, or replacing
Wine for arbitrary Windows setup.exe files.

## Dependencies

* **CMake** 3.x (2.8+ may still configure; CI/local builds use modern CMake)
* A C++17-capable compiler
* **liblzma** ([xz-utils](https://tukaani.org/xz/)) — **required** for modern Inno/GOG installers
* **zlib** and **bzip2** — older compression methods
* **iconv** *(optional)* — system libc, win32, or libiconv

On Windows, [vcpkg](https://vcpkg.io/) with a static triplet works well, e.g.:

```text
liblzma zlib bzip2
```

(see also Akhenaten’s `cmake/innoextract-vcpkg-deps.txt`).

## Build

```bash
mkdir build && cd build
cmake .. -DUSE_STATIC_LIBS=ON -DUSE_LZMA=ON -DBUILD_TESTS=OFF
cmake --build . --config Release
```

Windows + vcpkg example:

```powershell
cmake -S . -B build-noboost `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DUSE_STATIC_LIBS=ON -DUSE_LZMA=ON -DBUILD_TESTS=OFF
cmake --build build-noboost --config Release
```

### Useful CMake options

| Option | Default | Description |
|:---|:---:|:---|
| `USE_LZMA` | `ON` | liblzma support (keep on for GOG) |
| `BUILD_DECRYPTION` | `ON` | encrypted installer support |
| `USE_STATIC_LIBS` | `ON` on Windows | static link compression libs |
| `BUILD_TESTS` | `OFF` | unit tests (`make check`) |
| `DEVELOPER` | `OFF` | debug-oriented defaults |

## Run

```bash
innoextract -e -d ./out Setup.exe
```

```powershell
.\innoextract.exe -e -d .\PharaohData .\Installer\setup_pharaoh_gold_….exe
```

Help:

```bash
innoextract --help
```

### Regression helper (Akhenaten)

```powershell
.\tools\regression_extract.ps1 `
  -InnoextractPath path\to\innoextract.exe `
  -Installer C:\path\to\Setup.exe `
  -OutDir D:\tmp\extract-test
```

More notes: [tools/README-noboost.md](tools/README-noboost.md).

## Relationship to Akhenaten

Akhenaten builds this tree as an `ExternalProject` (`cmake/BuildInnoextract.cmake`), copies
`innoextract` next to the game, and may unpack `Installer/*.exe` into `PharaohData` on startup
when Steam data is missing.

InstallShield / Sierra demo packages are **out of scope** here; Akhenaten handles those with
`7z` + `unshield`.

## Limitations

Same practical limits as upstream:

* No full component selection UI; limited name filtering
* Included scripts / checks are not executed
* Directory mapping for Inno variables is hard-coded
* Multi-disk slice names must follow the standard scheme

Windows-only alternative: [innounp](http://innounp.sourceforge.net/).

## Disclaimer

Not affiliated with Inno Setup or jrsoftware.org.  
Upstream project: [dscharrer/innoextract](https://github.com/dscharrer/innoextract).
