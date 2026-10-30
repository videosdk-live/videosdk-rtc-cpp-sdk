#pragma once

// Public-symbol visibility.
//
// ELF and Mach-O export every non-static symbol by default, so off Windows this
// macro is effectively documentation (and future-proofs a -fvisibility=hidden
// build).
//
// Windows is the interesting one, and it cuts both ways. MSVC exports NOTHING
// from a DLL unless asked: without __declspec(dllexport) the DLL links fine but
// ships an empty export table. Point the same macro at a STATIC library and the
// opposite happens — __declspec(dllimport) makes the compiler emit __imp_
// references that no .lib contains, and every consumer dies on unresolved
// externals. CMakeLists.txt builds Windows as a static .lib, so the markers stay
// empty here and opt in explicitly: define VIDEOSDK_SHARED to build or consume a
// DLL (VIDEOSDK_BUILDING then picks the export side, and CMake already sets it
// while building the library itself).

#if defined(_WIN32) && defined(VIDEOSDK_SHARED)
#  ifdef VIDEOSDK_BUILDING
#    define VIDEOSDK_API __declspec(dllexport)
#  else
#    define VIDEOSDK_API __declspec(dllimport)
#  endif
#elif defined(_WIN32)
#  define VIDEOSDK_API
#else
#  define VIDEOSDK_API __attribute__((visibility("default")))
#endif
