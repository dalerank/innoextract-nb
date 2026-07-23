# Akhenaten noboost fork notes

Branch `akhenaten-noboost` aims to build innoextract **without Boost**.

## Remaining allowed dependencies

- **liblzma** (required for modern Inno/GOG installers)
- **zlib** / **bzip2** (older compression methods)

## Regression

```powershell
.\tools\regression_extract.ps1 `
  -InnoextractPath ..\Akhenaten\build\tools\innoextract\bin\innoextract.exe `
  -Installer C:\path\to\Pharaoh_Setup.exe `
  -OutDir D:\tmp\pharaoh-extract-test
```

Compare extract trees between boost and noboost builds when changing the stream pipeline.
