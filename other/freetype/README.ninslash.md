# Bundled FreeType for Windows

The headers and x86-64 runtime are FreeType 2.14.3, downloaded from
`https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz`.
The source archive SHA-256 is
`36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f`.

The DLL is built with MinGW-w64 as a release shared library, with Brotli,
BZip2, HarfBuzz, PNG, and zlib integration disabled. The target's Windows
prefix is set to an empty string so both the import library and executable
refer to `freetype.dll`. Its only runtime imports are `KERNEL32.dll` and
`msvcrt.dll`; the release verifier enforces this dependency closure.

The FreeType license is in `LICENSE.TXT`. Ninslash no longer ships a 32-bit
Windows build; the legacy `lib32` directory is not used by supported builds.
