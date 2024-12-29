set_project("AsmA65k")
set_version("0.2.0")
set_languages("cxx20")
add_rules("mode.debug", "mode.release")
set_warnings("all")

if is_mode("debug") then
    add_defines("DEBUG")
    set_symbols("debug")
    set_optimize("none")
else 
    set_symbols("hidden")
    set_optimize("fastest")
end

-- Platform specific settings
if is_plat("windows") then
    set_warnings("all")
    add_defines("WIN32")
    add_cxflags("/wd4068")
elseif is_plat("macosx") then
    add_defines("UNIX_HOST")
    add_cxflags("-stdlib=libc++")
elseif is_plat("linux") or is_plat("mingw") then
    add_defines("UNIX_HOST")
end

Target =
{
    standalone = 1,
    library = 2
}

local _target = Target.library

function AddCommon()
    add_includedirs("src")
    add_files("src/Asm65k.cpp")
    add_files("src/AsmA65k-Assembly.cpp")
    add_files("src/AsmA65k-Directives.cpp")
    add_files("src/AsmA65k-Misc.cpp")
    set_targetdir("bin")
end

if _target == Target.standalone then
    target("AsmA65k")
        AddCommon()
        add_files("src/main.cpp")
        set_kind("binary")
elseif _target == Target.library then
    target("AsmA65k-lib")
        AddCommon()
        set_kind("static")
end
