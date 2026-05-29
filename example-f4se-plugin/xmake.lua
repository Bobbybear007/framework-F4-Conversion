-- example-f4se-plugin/xmake.lua
add_requires("spdlog", "fmt", "boost")

target("PrismaUI-F4-Example-Plugin")
    set_kind("shared")
    set_languages("c++23")
    set_filename("PrismaUI-F4-Example-Plugin.dll")

    add_includedirs("../../../CommonLibF4_OG/CommonLibF4/include")
    add_includedirs("../../../CommonLibF4_OG/lib/mmio/include")
    add_includedirs("src")

    add_files("src/**.cpp")
    add_packages("boost", "spdlog", "fmt")

    add_linkdirs("../../../CommonLibF4_OG/build/windows/x64/release")
    add_links("commonlibf4", "mmio")

    set_pcxxheader("src/PCH.h")

    add_defines("WINVER=0x0601", "_WIN32_WINNT=0x0601")
    add_defines("F4SE_SUPPORT_820")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("Version", "Ole32", "OleAut32", "User32", "bcrypt", "crypt32")
    end
