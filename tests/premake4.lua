-- premake4.lua
--solution "lkchecker"
--   configurations { "Release" }

project "functions"
   kind "ConsoleApp"
   language "C"

   files { "unittest.h", "functions.c" }
   includedirs { "../lib/include" }
   objdir "../obj/tests"
   targetdir "../out/"
   links { "lkchecker" }

   configurations "Release"
      defines {  }

project "dictfuncs"
   kind "ConsoleApp"
   language "C"

   files { "unittest.h", "dictfuncs.c" }
   includedirs { "../lib/include" }
   objdir "../obj/tests"
   targetdir "../out/"
   links { "lkchecker" }

   configurations "Release"
      defines {  }

project "treefuncs"
   kind "ConsoleApp"
   language "C"

   files { "unittest.h", "treefuncs.c" }
   includedirs { "../lib/include" }
   objdir "../obj/tests"
   targetdir "../out/"
   links { "lkchecker" }

   configurations "Release"
      defines {  }
