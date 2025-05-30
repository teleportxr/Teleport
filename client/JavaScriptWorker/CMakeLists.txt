cmake_minimum_required(VERSION 3.8)
project(JavaScriptWorker)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	# We'll get Boost, so we can have Boost Interprocess.
	if(${TELEPORT_BOOST})
		set(BOOST_INCLUDE_LIBRARIES interprocess)
		set(BOOST_ENABLE_CMAKE ON)
		include(FetchContent)
		FetchContent_Declare(
		  boost
		  GIT_REPOSITORY https://github.com/boostorg/boost.git
		  GIT_TAG boost-1.87.0
		)
		FetchContent_MakeAvailable(boost)
	endif()
	#add_executable(boost_test boost_test.cpp)
	#target_link_libraries(boost_test PRIVATE Boost::filesystem
	#										 Boost::program_options)

	#################################################################
	#				V8												#
	#################################################################
	find_program(NUGET nuget)
	if(NOT NUGET)
		message(FATAL_ERROR "CMake could not find the nuget command line tool. Please install it!")
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
	
	#################################################################
	#				JavaScriptWorker								#
	#################################################################
	#add_executable( JavaScriptWorker  main.cpp packages.config.in)
	add_static_executable(JavaScriptWorker CONSOLE SOURCES Main.cpp SceneObjectModel.cpp SceneObjectModel.h packages.config.in index.rst FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
	set_target_properties( JavaScriptWorker PROPERTIES FOLDER Client)
	# nuget puts the packages in the build root/packages
	# ${CMAKE_BINARY_DIR})/packages/v8-v143-x64.9.8.177.4/include
	target_include_directories(JavaScriptWorker PUBLIC ${TELEPORT_V8_PACKAGE_DIR}/include)
	target_link_directories(JavaScriptWorker PUBLIC ${TELEPORT_V8_PACKAGE_DIR}/lib/Release)
	target_link_libraries(JavaScriptWorker v8.dll.lib v8_libbase.dll.lib v8_libplatform.dll.lib zlib.dll.lib)
	# Definitions for compatibility with the v8 dll's:
	target_compile_definitions(JavaScriptWorker PUBLIC V8_COMPRESS_POINTERS V8_31BIT_SMIS_ON_64BIT_ARCH)
		
	
	#################################################################
	#				JavaScriptTest									#
	#################################################################
	# Now we create a test host that will create and run instances of the runner.
	add_static_executable(JavaScriptTest CONSOLE SOURCES Test.cpp test.js FOLDER ${CMAKE_CURRENT_SOURCE_DIR} )
	target_link_libraries(JavaScriptTest TeleportClient)
	target_include_directories(JavaScriptTest PUBLIC ${CMAKE_SOURCE_DIR})
	set_target_properties(JavaScriptTest PROPERTIES FOLDER Client)
	add_dependencies(JavaScriptTest JavaScriptWorker)

	# Provide the path to the Nuget-installed references, in case this is a fresh project build.
   #set_property(TARGET JavaScriptWorker PROPERTY 
   #	 VS_DOTNET_REFERENCE_MyReferenceLib
	#	${CMAKE_BINARY_DIR}/packages/path/to/lib/MyReferenceLib.dll)
   # set_property(TARGET JavaScriptWorker
	#	PROPERTY VS_PACKAGE_REFERENCES "v8-v143-x64"
   # )
endif()
#nuget install v8-v143-x64 -OutputDirectory ${{github.workspace}}/build