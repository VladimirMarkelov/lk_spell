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

project "textparse"
   kind "ConsoleApp"
   language "C"

   if not _OPTIONS["utf8proc_inc"] then
       includedirs { "../lib/include" }
   else
       includedirs { _OPTIONS["utf8proc_inc"], "../lib/include" }
   end
   if _OPTIONS["utf8proc_lib"] then
       libdirs { _OPTIONS["utf8proc_lib"] }
   end

   files { "textparse.c" }
   objdir "../obj/utils"
   targetdir "../out/"
   links { "utf8proc", "lkchecker" }
