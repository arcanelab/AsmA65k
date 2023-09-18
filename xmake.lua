set_project("AsmA65k")
set_version("0.1.0")
set_languages("cxx20")
add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
else
    set_symbols("hidden")
    set_optimize("fastest")
end

target("AsmA65k")
    set_kind("binary")
    -- set_targetdir("lib")
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")
    add_includedirs("src")
    set_targetdir("$(projectdir)")
