# Developer notes (noboost)

This file is a short companion to the main [README](../README.md).

## Allowed deps

- **liblzma** — modern Inno / GOG
- **zlib** / **bzip2** — older compression
- **No Boost**

## Mechanical rewrite helper

`deboost_mechanical.py` was used during the Boost removal pass (include / API substitutions).
Prefer manual review for stream / CLI changes; do not re-run blindly on an already-converted tree.

## Regression

```powershell
.\tools\regression_extract.ps1 `
  -InnoextractPath path\to\innoextract.exe `
  -Installer C:\path\to\Pharaoh_Setup.exe `
  -OutDir D:\tmp\pharaoh-extract-test
```

Compare trees against an upstream (Boost) build when changing the stream pipeline.
