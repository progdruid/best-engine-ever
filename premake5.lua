-- premake5.lua


-- cleaning previously generated files and folders
local function cleanGenerated()
    -- Only remove build dirs at repo root (do not touch vendor/)
    os.rmdir("bin")
    os.rmdir("bin-int")
    os.rmdir("obj")

    -- Only remove VS files at root and in known project dirs, not globally
    os.remove("./*.sln")                 
    os.remove("./*.vcxproj")             
    os.remove("./*.vcxproj.filters")     
    os.remove("./*.vcxproj.user")        
    os.remove("./*.vcxitems")            
    os.remove("./*.vcxitems.filters")    
end

if _ACTION == "vs2022" or _ACTION == "vs2019" then
    cleanGenerated()
end

-- workspace and project definitions
workspace "DXSandbox"
    configurations { "Debug", "Release" }
    system "windows"
    architecture "x86_64"  
    location "."                         

    externalproject "DirectXTK"
        location "vendor/DirectXTK/"
        filename "DirectXTK_Desktop_2022"
        uuid "1C4D4C2C-7D35-4BA3-A2B7-4F3B6C2E9B10" -- picked a stable GUID for solution
        kind "StaticLib"
        language "C++"

project "Engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    targetdir ("%{wks.location}/bin/%{cfg.platform}/%{cfg.buildcfg}")
    objdir    ("%{wks.location}/obj/%{cfg.platform}/%{cfg.buildcfg}")

    files { 
        "src/**.cpp", 
        "src/**.c", 
        "src/**.h", 
        "src/**.hpp",
        "src/**.hlsl", 
    }
    

    includedirs { 
        "src", 
        "vendor/glfw/include", 
        "vendor/glm", 
        "vendor/DirectXTK/Inc" 
    }
    libdirs { 
        "vendor/glfw/lib-vc2022" 
    }
    links { 
        "DirectXTK", 
        "glfw3", 
        "d3d11", 
        "dxgi", 
        "d3dcompiler" 
    }

    -- exclude HLSL files from build / not building shaders yet
    filter { "files:src/**.hlsl" }
        flags { "ExcludeFromBuild" }

    -- our debug configuration
    filter "configurations:Debug"
        symbols "On"
        defines { "DEBUG" }
        optimize "Off"

    -- our release configuration
    filter "configurations:Release"
        symbols "Off"
        defines { "NDEBUG" }
        optimize "Full"

    -- idk
    filter { "toolset:msc*", "language:C++" }
        buildoptions { "/Zc:__cplusplus" }

    filter {}



-- a utility project to manage misc files
project "MiscConfiguration"
    kind "Utility"
    files {
        "premake5.lua",
        ".gitignore",
        "README.md"
    }