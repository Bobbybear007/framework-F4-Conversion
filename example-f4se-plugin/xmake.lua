-- example-f4se-plugin/xmake.lua
--
-- CommonLibF4 is included as a submodule at lib/commonlibf4.
-- Clone with:  git clone --recurse-submodules https://github.com/NomadsReach/framework-F4-Conversion
-- Then build:  xmake -P example-f4se-plugin
--
-- Optional deploy env vars:
--   XSE_FO4_MODS_PATH  — MO2 mods folder root
--   XSE_FO4_GAME_PATH  — Fallout 4 Data folder

includes("../lib/commonlibf4")

target("PrismaUI-F4-Example-Plugin")
    set_kind("shared")
    set_languages("c++23")
    set_filename("PrismaUI-F4-Example-Plugin.dll")

    -- commonlibf4.plugin rule: generates F4SE_PLUGIN_VERSION data and sets install path
    -- from XSE_FO4_MODS_PATH / XSE_FO4_GAME_PATH env vars.
    add_rules("commonlibf4.plugin", {
        name    = "PrismaUI-F4-Example-Plugin",
        author  = "PrismaUI",
        version = "1.0.0"
    })

    add_includedirs("src")
    add_files("src/**.cpp")

    set_pcxxheader("src/PCH.h")

    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("Version", "Ole32", "OleAut32", "User32", "bcrypt", "crypt32")
    end
