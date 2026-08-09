# Bundled FreeType for Windows

The headers and x86/x86-64 runtimes are FreeType 2.14.3, downloaded from
`https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz`.
The source archive SHA-256 is
`36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f`.

The DLLs are built with MinGW-w64 as release shared libraries, with Brotli,
BZip2, HarfBuzz, PNG, and zlib integration disabled. The x86 DLL is built with
`-static-libgcc` so it does not require the legacy `MSVCR100.dll`; both target
architectures only import `KERNEL32.dll` and `msvcrt.dll`. The release verifier
enforces this dependency closure.

The FreeType license is in `LICENSE.TXT`. The `lib32` and `lib64` directories
contain the matching Windows runtimes selected by CMake.
