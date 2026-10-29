-- premake5.lua

-- folder with the installed game (DATA\, LANG.*, config.dat ...), used as the debugger working dir
DRIVER_DATA_DIR = os.getenv("DRIVER_DATA_DIR") or "../data"

workspace "OpenDriver1"
	location "build"
	configurations { "Debug", "Release" }
	platforms { "x86" }		-- the game is 32-bit only (inline asm, DirectX 6/7, pointers in data)
	startproject "Driver"

	language "C"
	characterset "MBCS"

	defines {
		"WIN32",
		"_WINDOWS",
		"_CRT_SECURE_NO_WARNINGS",
		-- [A] FPUInit leaves the FPU as it is (no single precision chop mode; float_to_int truncates as
		-- under chop). Drop it to run with the original FPU mode once all the code is rebuilt.
		"OPENDRIVER_NO_FPU_CHOP",
	}

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
		"dependency/glideSDK/glide2x/include",
	}

	files {
		"game6.4g/**.c",
		"game6.4g/**.h",
		"stubs/**.c",
		"stubs/**.h",
	}

	links {
		"ddraw",
		"dxguid",
		"dinput8",	-- the modern SDK has no dinput.lib; revisit when input.c is translated
		"dsound",
		"winmm",
		"ole32",
	}

	-- inline asm and hand-written stack frames
	linkoptions { "/SAFESEH:NO" }

	debugdir (DRIVER_DATA_DIR)
	debugargs { "/WINDOW" }	-- [A] windowed mode, see gWindowed in shell.c

	-- DirectDraw wrapper next to the exe: the system ddraw.dll offers no hardware D3D device in an
	-- RDP session (and none at all on many modern setups), so CanUse3DDevice finds nothing.
	-- dgVoodoo from the game folder provides one.
	local dgvoodoo = DRIVER_DATA_DIR .. "/dgvoodoo"
	if os.isfile(dgvoodoo .. "/DDraw.dll") then
		local src = path.translate(path.getabsolute(dgvoodoo))
		postbuildcommands {
			'{COPYFILE} "' .. src .. '\\DDraw.dll" "%{cfg.buildtarget.directory}"',
			'{COPYFILE} "' .. src .. '\\D3DImm.dll" "%{cfg.buildtarget.directory}"',
			'{COPYFILE} "' .. src .. '\\dgVoodoo.conf" "%{cfg.buildtarget.directory}"',
		}
	end
