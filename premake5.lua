workspace "Dx12MemAllocator"
	architecture "x86_64"
	startproject "Dx12MemAllocator"

	configurations {
		"Debug",
		"Release",
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Dx12MemAllocator"
	location "Dx12MemAllocator"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"%{prj.name}/tests/**.cpp",
		"%{prj.name}/include/**.h",
		"%{prj.name}/googletest/include/**.h",
		"%{prj.name}/googletest/src/gtest-all.cc",
	}

	includedirs {
		"%{prj.name}/include",
		"%{prj.name}/googletest",
		"%{prj.name}/googletest/include",
	}

	links {
		"d3d12.lib",
		"dxgi.lib"
	}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"