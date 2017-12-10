-- premake4.lua
--solution "lkchecker"
--   configurations { "Release" }

project "lkchecker"
   kind "SharedLib"
   language "C"

   files { "**.h", "src/utf8proc.c", "src/filereader.c",
   		   "src/dictionary.c", "src/wordutils.c", "src/suftree.c" }
   includedirs { "./include" }
   objdir "../obj/lkchecker"
   targetdir "../out/"

   configurations "Release"
      defines { "NDEBUG" }
      flags { "Optimize" }
