cmake_minimum_required(VERSION 3.8)
project(JavaScriptWorker)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

	# Get the "safe" header library for lockable resources
	#FetchContent_Declare(
	#  v8pp
	#  GIT_REPOSITORY https://github.com/pmed/v8pp.git
	#	GIT_SHALLOW 1
	#  GIT_TAG		v1.1.0
	#  SOURCE_DIR v8pp
	#)
	#FetchContent_MakeAvailable(v8pp)

	#add_executable( JavaScriptRunner  main.cpp packages.config.in)
	add_static_executable(JavaScriptRunner CONSOLE SOURCES main.cpp packages.config.in FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
	set_target_properties( JavaScriptRunner PROPERTIES FOLDER Client)
	find_program(NUGET nuget)
	if(NOT NUGET)
		message("CMake could not find the nuget command line tool. Please install it!")
	endif()
	set(TELEPORT_V8_VERSION 9.8.177.4)
	set(TELEPORT_V8_PACKAGE_DIR ${CMAKE_BINARY_DIR}/packages/v8-v143-x64.${TELEPORT_V8_VERSION})
	set(TELEPORT_V8_PACKAGE_REDIST_DIR ${CMAKE_BINARY_DIR}/packages/v8.redist-v143-x64.${TELEPORT_V8_VERSION})
	message( "Found NuGet  ${NUGET}")
		# Copy the Nuget config file from source location to the CMake build directory.
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packages.config.in ${CMAKE_CURRENT_BINARY_DIR}/packages.config COPYONLY)
	message("configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packages.config.in ${CMAKE_CURRENT_BINARY_DIR}/packages.config COPYONLY)")
		# Run Nuget using the .config file to install any missing dependencies to the build directory.
	execute_process(COMMAND 
			${NUGET} restore ${CMAKE_CURRENT_BINARY_DIR}/packages.config -SolutionDirectory ${CMAKE_BINARY_DIR}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
		# deploy the dll's to the bin directory.
	set(TELEPORT_V8_DLLS icui18n icuuc v8 v8_libbase v8_libplatform zlib)
	foreach(dll ${TELEPORT_V8_DLLS})
		configure_file(${TELEPORT_V8_PACKAGE_REDIST_DIR}/lib/Release/${dll}.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/${dll}.dll COPYONLY)
	endforeach()
		# nuget puts the packages in the build root/packages
		# ${CMAKE_BINARY_DIR})/packages/v8-v143-x64.9.8.177.4/include
	target_include_directories(JavaScriptRunner PUBLIC ${TELEPORT_V8_PACKAGE_DIR}/include)
	target_link_directories(JavaScriptRunner PUBLIC ${TELEPORT_V8_PACKAGE_DIR}/lib/Release)
	target_link_libraries(JavaScriptRunner v8.dll.lib v8_libbase.dll.lib v8_libplatform.dll.lib zlib.dll.lib)
	# Definitions for compatibility with the v8 dll's:
	target_compile_definitions(JavaScriptRunner PUBLIC V8_COMPRESS_POINTERS V8_31BIT_SMIS_ON_64BIT_ARCH)
		
	# Now we create a test host that will create and run instances of the runner.
	add_static_executable(JavaScriptTest CONSOLE SOURCES Test.cpp FOLDER ${CMAKE_CURRENT_SOURCE_DIR} )
	
	set_target_properties(JavaScriptTest PROPERTIES FOLDER Client)
	add_dependencies(JavaScriptTest JavaScriptRunner)
	# Provide the path to the Nuget-installed references, in case this is a fresh project build.
   #s#et_property(TARGET JavaScriptRunner PROPERTY 
   #	 VS_DOTNET_REFERENCE_MyReferenceLib
	#	${CMAKE_BINARY_DIR}/packages/path/to/lib/MyReferenceLib.dll)
   # set_property(TARGET JavaScriptRunner
	#	PROPERTY VS_PACKAGE_REFERENCES "v8-v143-x64"
   # )
endif()
#nuget install v8-v143-x64 -OutputDirectory ${{github.workspace}}/build