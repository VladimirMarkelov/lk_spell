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

project "lkchecker"
   kind "SharedLib"
   language "C"

   files { "**.h", "**.c" }

   if not _OPTIONS["utf8proc_inc"] then
       includedirs { "include" }
   else
       includedirs { _OPTIONS["utf8proc_inc"], "include" }
   end
   if _OPTIONS["utf8proc_lib"] then
       libdirs { _OPTIONS["utf8proc_lib"] }
   end

   objdir "../obj/lkchecker"
   targetdir "../out/"

   configuration "Release"
      defines { "NDEBUG" }
      flags { "Optimize" }

   links { "utf8proc" }
