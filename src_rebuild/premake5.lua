-- premake5.lua

-- folder with the installed game (DATA\, LANG.*, config.dat ...), used as the debugger working dir
DRIVER_DATA_DIR = os.getenv("DRIVER_DATA_DIR") or "../data"

workspace "OpenDriver1"
	location "build"
	configurations { "Debug", "Release" }
	platforms { "x86" }		-- the game is 32-bit only (inline asm, pointers in data)
	startproject "Driver"

	language "C"
	characterset "MBCS"

	defines {
		"WIN32",
		"_WINDOWS",
		"_CRT_SECURE_NO_WARNINGS",
		"SDL_MAIN_HANDLED",		-- the game has its own main, see the entry point below
		-- the OPENDRIVER_ switches of the [A] changes are in game6.4g/dr_globals.h
	}

	-- float code on the x87 FPU, as in the original: FPUInit sets single precision with chop
	-- rounding and every float operation rounds that way (SSE2 code would round to nearest, and
	-- the road heights / normals in MapHeight, FindSurface would differ by a unit now and then)
	vectorextensions "IA32"

	buildoptions {
		"/J",		-- char is unsigned, as in the original build
	}

	-- static CRT (/MT, /MTd) like the game (its debug CRT is linked in)
	staticruntime "On"

	filter "platforms:x86"
		architecture "x86"

	filter "configurations:Debug"
		defines { "_DEBUG", "OPENDRIVER_DEBUG" }	-- OPENDRIVER_DEBUG: our extra diagnostics ([A])
		symbols "On"
		optimize "Off"

	filter "configurations:Release"
		defines { "NDEBUG" }
		symbols "On"
		optimize "Speed"
		stringpooling "On"
		functionlevellinking "On"

	filter {}

project "Driver"
	kind "WindowedApp"
	targetdir "bin/%{cfg.buildcfg}"
	targetname "Driver"

	includedirs {
		"game6.4g",
		"dependency",
		"dependency/glideSDK/glide2x/include",
		"dependency/SDL2/include",
		"dependency/openal-soft/include",
		"dependency/glad/include",
	}

	libdirs {
		"dependency/SDL2/lib/x86",
		"dependency/openal-soft/lib/x86",
	}

	files {
		"game6.4g/**.c",
		"game6.4g/**.cpp",
		"game6.4g/**.h",
		"dependency/stb/stb_vorbis.c",
		"dependency/glad/src/glad.c",
	}

	links {
		"SDL2",
		"OpenAL32",
	}

	-- main() with no console, without SDL2main (which would bring its own CRT settings)
	entrypoint "mainCRTStartup"

	-- inline asm and hand-written stack frames
	linkoptions { "/SAFESEH:NO" }

	debugdir (DRIVER_DATA_DIR)

	for _, dll in ipairs({ "dependency/SDL2/lib/x86/SDL2.dll", "dependency/openal-soft/lib/x86/OpenAL32.dll" }) do
		postbuildcommands {
			'{COPYFILE} "' .. path.translate(path.getabsolute(dll)) .. '" "%{cfg.buildtarget.directory}"',
		}
	end

