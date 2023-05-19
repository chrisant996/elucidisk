local to = ".build/"..(_ACTION or "nullaction")

if _ACTION == "gmake2" then
    error("Use `premake5 gmake` instead; gmake2 neglects to link resources.")
end


--------------------------------------------------------------------------------
local function init_configuration(cfg)
    configuration(cfg)
        defines("BUILD_"..cfg:upper())
        targetdir(to.."/bin/%{cfg.buildcfg}/%{cfg.platform}")
        objdir(to.."/obj/")
end

--------------------------------------------------------------------------------
local function define_exe(name, exekind)
    project(name)
    flags("fatalwarnings")
    language("c++")
    kind(exekind or "consoleapp")
end


--------------------------------------------------------------------------------
workspace("elucidisk")
    configurations({"debug", "release"})
    platforms({"x64"})
    location(to)

    characterset("Unicode")
    flags("NoManifest")
    staticruntime("on")
    symbols("on")
    exceptionhandling("off")

    init_configuration("release")
    init_configuration("debug")

    configuration("debug")
        rtti("on")
        optimize("off")
        defines("DEBUG")
        defines("_DEBUG")

    configuration("release")
        rtti("off")
        optimize("full")
        omitframepointer("on")
        defines("NDEBUG")

    configuration({"release", "vs*"})
        flags("LinkTimeOptimization")

    configuration("vs*")
        defines("_HAS_EXCEPTIONS=0")

--------------------------------------------------------------------------------
project("elucidisk")
    targetname("elucidisk")
    kind("windowedapp")
    links("comctl32")
    links("d2d1")

    language("c++")
    flags("fatalwarnings")

    files("data.cpp")
    files("scan.cpp")
    files("sunburst.cpp")
    files("actions.cpp")
    files("ui.cpp")
    files("dpi.cpp")
    files("main.cpp")
    files("version.rc")

    configuration("vs*")
        defines("_CRT_SECURE_NO_WARNINGS")
        defines("_CRT_NONSTDC_NO_WARNINGS")



--------------------------------------------------------------------------------
local any_warnings_or_failures = nil

--------------------------------------------------------------------------------
local release_manifest = {
    "elucidisk.exe",
}

--------------------------------------------------------------------------------
local function warn(msg)
    print("\x1b[0;33;1mWARNING: " .. msg.."\x1b[m")
    any_warnings_or_failures = true
end

--------------------------------------------------------------------------------
local function failed(msg)
    print("\x1b[0;31;1mFAILED: " .. msg.."\x1b[m")
    any_warnings_or_failures = true
end

--------------------------------------------------------------------------------
local exec_lead = "\n"
local function exec(cmd, silent)
    print(exec_lead .. "## EXEC: " .. cmd)

    if silent then
        cmd = "1>nul 2>nul "..cmd
    else
        -- cmd = "1>nul "..cmd
    end

    -- Premake replaces os.execute() with a version that runs path.normalize()
    -- which converts \ to /.  That can cause problems when executing some
    -- programs such as cmd.exe.
    local prev_norm = path.normalize
    path.normalize = function (x) return x end
    local _, _, ret = os.execute(cmd)
    path.normalize = prev_norm

    return ret == 0
end

--------------------------------------------------------------------------------
local function exec_with_retry(cmd, tries, delay, silent)
    while tries > 0 do
        if exec(cmd, silent) then
            return true
        end

        tries = tries - 1

        if tries > 0 then
            print("... waiting to retry ...")
            local target = os.clock() + delay
            while os.clock() < target do
                -- Busy wait, but this is such a rare case that it's not worth
                -- trying to be more efficient.
            end
        end
    end

    return false
end

--------------------------------------------------------------------------------
local function mkdir(dir)
    if os.isdir(dir) then
        return
    end

    local ret = exec("md " .. path.translate(dir), true)
    if not ret then
        error("Failed to create directory '" .. dir .. "' ("..tostring(ret)..")", 2)
    end
end

--------------------------------------------------------------------------------
local function rmdir(dir)
    if not os.isdir(dir) then
        return
    end

    return exec("rd /q /s " .. path.translate(dir), true)
end

--------------------------------------------------------------------------------
local function unlink(file)
    return exec("del /q " .. path.translate(file), true)
end

--------------------------------------------------------------------------------
local function copy(src, dest)
    src = path.translate(src)
    dest = path.translate(dest)
    return exec("copy /y " .. src .. " " .. dest, true)
end

--------------------------------------------------------------------------------
local function rename(src, dest)
    src = path.translate(src)
    return exec("ren " .. src .. " " .. dest, true)
end

--------------------------------------------------------------------------------
local function file_exists(name)
    local f = io.open(name, "r")
    if f ~= nil then
        io.close(f)
        return true
    end
    return false
end

--------------------------------------------------------------------------------
local function have_required_tool(name, fallback)
    if exec("where " .. name, true) then
        return name
    end

    if fallback then
        local t
        if type(fallback) == "table" then
            t = fallback
        else
            t = { fallback }
        end
        for _,dir in ipairs(t) do
            local file = dir .. "\\" .. name .. ".exe"
            if file_exists(file) then
                return '"' .. file .. '"'
            end
        end
    end

    return nil
end

--------------------------------------------------------------------------------
newaction {
    trigger = "release",
    description = "Creates a release of elucidisk",
    execute = function ()
        local premake = _PREMAKE_COMMAND
        local root_dir = path.getabsolute(".build/release") .. "/"

        -- Check we have the tools we need.
        local have_msbuild = have_required_tool("msbuild", { "c:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\MSBuild\\Current\\Bin", "c:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\MSBuild\\Current\\Bin" })
        local have_7z = have_required_tool("7z", { "c:\\Program Files\\7-Zip", "c:\\Program Files (x86)\\7-Zip" })

        -- Clone repo in release folder and checkout the specified version
        local code_dir = root_dir .. "~working/"
        rmdir(code_dir)
        mkdir(code_dir)

        exec("git clone . " .. code_dir)
        if not os.chdir(code_dir) then
            error("Failed to chdir to '" .. code_dir .. "'")
        end
        exec("git checkout " .. (_OPTIONS["commit"] or "HEAD"))

        -- Build the code.
        local x86_ok = true
        local x64_ok = true
        local arm64_ok = true
        local toolchain = "ERROR"
        local build_code = function (target)
            if have_msbuild then
                target = target or "build"

                toolchain = _OPTIONS["vsver"] or "vs2019"
                exec(premake .. " " .. toolchain)
                os.chdir(".build/" .. toolchain)

                x86_ok = exec(have_msbuild .. " /m /v:q /p:configuration=release /p:platform=win32 elucidisk.sln /t:" .. target)
                x64_ok = exec(have_msbuild .. " /m /v:q /p:configuration=release /p:platform=x64 elucidisk.sln /t:" .. target)

                os.chdir("../..")
            else
                error("Unable to locate msbuild.exe")
            end
        end

        -- Build everything.
        build_code()

        local src = path.getabsolute(".build/" .. toolchain .. "/bin/release").."/"

        -- Do a coarse check to make sure there's a build available.
        if not os.isdir(src .. ".") or not (x86_ok or x64_ok) then
            error("There's no build available in '" .. src .. "'")
        end

        -- Parse version.
        local ver_file = io.open("version.h")
        if not ver_file then
            error("Failed to open version.h file")
        end
        local vmaj, vmin, vpat
        for line in ver_file:lines() do
            if not vmaj then
                vmaj = line:match("VERSION_MAJOR%s+([^%s+])")
            end
            if not vmin then
                vmin = line:match("VERSION_MINOR%s+([^%s+])")
            end
            if not vpat then
                vpat = line:match("VERSION_PATCH%s+([^%s+])")
            end
        end
        ver_file:close()
        if not (vmaj and vmin and vpat) then
            error("Failed to get version info")
        end
        local version = vmaj .. "." .. vmin .. "." .. vpat
        
        -- Now we know the version we can create our output directory.
        local target_dir = root_dir .. os.date("%Y%m%d_%H%M%S") .. "_" .. version .. "/"
        rmdir(target_dir)
        mkdir(target_dir)

        -- Package the release and the pdbs separately.
        os.chdir(src .. "/x64")
        if have_7z then
            exec(have_7z .. " a -r  " .. target_dir .. "/elucidisk-x64-v" .. version .. "-pdb.zip  *.pdb")
            exec(have_7z .. " a -r  " .. target_dir .. "/elucidisk-x64-v" .. version .. "-exe.zip  *.exe")
        end

        -- Tidy up code directory.
        os.chdir(code_dir)
        rmdir(".build")
        rmdir(".git")
        unlink(".gitignore")

        -- Report some facts about what just happened.
        print("\n\n")
        if not have_7z then     warn("7-ZIP NOT FOUND -- Packing to .zip files was skipped.") end
        if not x64_ok then      failed("x64 BUILD FAILED") end
        if not any_warnings_or_failures then
            print("\x1b[0;32;1mRelease " .. version .. " built successfully.\x1b[m")
        end
    end
}

