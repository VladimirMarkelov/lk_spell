-- premake4.lua
--solution "lkchecker"
--   configurations { "Release" }

--#!lua
newoption {
    trigger = "utf8proc_inc",
    description = "Path to directory containing utf8proc headers",
    value = "path"
}

--#!lua
newoption {
    trigger = "utf8proc_lib",
    description = "utf8proc library path",
    value = "path"
}

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

   if not _OPTIONS["utf8proc_inc"] then
       includedirs { "../lib/include" }
   else
       includedirs { _OPTIONS["utf8proc_inc"], "../lib/include" }
   end
   if _OPTIONS["utf8proc_lib"] then
       libdirs { _OPTIONS["utf8proc_lib"] }
   end

   objdir "../obj/tests"
   targetdir "../out/"
   links { "utf8proc", "lkchecker" }

   configurations "Release"
      defines {  }
