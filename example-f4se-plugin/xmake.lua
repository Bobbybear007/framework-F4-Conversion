-- example-f4se-plugin/xmake.lua
--
-- Uses NewCommonLib (E:\F4SE OG\Prisma\NewCommonLib), the xmake-based CommonLibF4.
-- Build NewCommonLib first: cd into it and run `xmake`.
-- Set XSE_FO4_MODS_PATH or XSE_FO4_GAME_PATH env vars to auto-deploy on `xmake install`.

-- NewCommonLib pulls spdlog v1.16.0 (wchar+std_format) internally via its own
-- add_requires. Do NOT add a separate spdlog requirement here — version or config
-- conflicts with the sub-project's requirement cause LOG.cpp compile errors.

-- CommonLibF4 is included by the root xmake.lua based on PRISMA_TARGET

target("PrismaUI-F4-Example-Plugin")
    set_kind("shared")
    set_languages("c++23")
    set_filename("PrismaUI-F4-Example.dll")

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

    -- NOTE: Ultralight libs and PrismaUI_F4 framework are deployed by PrismaUI_F4.dll build, not here.
    -- This plugin only needs to deploy its own DLL. Use build.bat or deploy.bat to copy to MO2.
