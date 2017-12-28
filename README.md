## About
Spellchecker (alpha version)

It kind of works but:
1. At this moment the code is in early alpha: it needs optimizing, refactoring, cleaning up etc.
2. No dictionary included, so it is not possible to spellcheck anything
3. Some function are to be removed - now they exist just for debugging

## How to build
### Building utf8proc
Clone the sources from [utf8proc](https://github.com/JuliaLang/utf8proc). The library needs CMake to build.

On Linux it does not require anything special else - just build and install with CMake.

On Windows, without patching header files, the library can be used only as DLL. But by default CMake builds
a static library. To create a DLL with CMake you have to add a CMake variable BUILD_SHARED_LIBS and set it to True.
After adding variable build the library in the standard way: configure and then generate makefiles.

### Building lk_spell
If utf8proc library and headers are not in common location you should tell premake4 where they are. The example
command line to build the library with MinGW looks like:
```
premake4 --utf8proc_inc=<path_to_folder_with_utf8proc_headers> --utf8proc_lib=<path_to_folder_with_libutf8proc.a> gmake
```

### TODO
- [ ] Add at least small dictionary to the library
- [ ] Add documentation about how to use the library
- [ ] Test on Linux distribution
- [ ] ? Improve spelling suggestions
- [ ] ? Optimizations
