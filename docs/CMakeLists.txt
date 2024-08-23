cmake_minimum_required(VERSION 3.22)
project(Teleport_Docs)
find_package(Doxygen REQUIRED)
cmake_path(CONVERT "${CMAKE_CURRENT_SOURCE_DIR}/../" TO_CMAKE_PATH_LIST TELEPORT_ROOT_DIR NORMALIZE)
# Find all the public headers
#message("TELEPORT_ROOT_DIR ${TELEPORT_ROOT_DIR}")
set(pc_client_HEADER_DIRS ${TELEPORT_ROOT_DIR}/pc_client)
set(AndroidClient_HEADER_DIRS ${TELEPORT_ROOT_DIR}/AndroidClient)
#message("pc_client_HEADER_DIRS ${pc_client_HEADER_DIRS}")
set(AV ${TELEPORT_ROOT_DIR}/libavstream/include/libavstream)
set(libavstream_HEADER_DIRS ${AV} ${AV}/audio ${AV}/decoders ${AV}/encoders ${AV}/geometry ${AV}/network ${AV}/platforms ${AV}/stream ${AV}/surfaces)

set(teleportclient_HEADER_DIRS ${TELEPORT_ROOT_DIR}/TeleportClient)
set(teleportcore_HEADER_DIRS ${TELEPORT_ROOT_DIR}/TeleportCore)

set(teleportrenderer_DIR	${TELEPORT_ROOT_DIR}/ClientRender)
set(teleportrenderer_HEADER_DIRS ${teleportrenderer_DIR} ${teleportrenderer_DIR}/NodeComponents ${teleportrenderer_DIR}/AVParser)

set(teleportserver_HEADER_DIRS ${TELEPORT_ROOT_DIR}/TeleportServer ${TELEPORT_ROOT_DIR}/TeleportServer/UnityPlugin ${TELEPORT_ROOT_DIR}/TeleportServer/UnityPlugin/PluginMain.cpp ${TELEPORT_ROOT_DIR}/TeleportServer/UnityPlugin/PluginClient.cpp)
set(DOXYGEN_INPUT_DIR ${pc_client_HEADER_DIRS} ${AndroidClient_HEADER_DIRS} ${libavstream_HEADER_DIRS} ${teleportcore_HEADER_DIRS} ${teleportclient_HEADER_DIRS} ${teleportrenderer_HEADER_DIRS} ${teleportserver_HEADER_DIRS})
#message("DOXYGEN_INPUT_DIR ${DOXYGEN_INPUT_DIR}")
string (REPLACE ";" "\",\"" DOXYGEN_INPUT_DIR "${DOXYGEN_INPUT_DIR}")
string (REPLACE "/" "\\" DOXYGEN_INPUT_DIR "${DOXYGEN_INPUT_DIR}")
#file(GLOB_RECURSE PUBLIC_HEADERS ${pc_client_HEADER_DIR}/*.h)
set(DOXYGEN_INPUT_DIR "\"${DOXYGEN_INPUT_DIR}\"")
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/xml.html)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) #Doxygen won't create this for us
add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
					#DEPENDS ${PUBLIC_HEADERS}
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
					MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
					COMMENT "Generating docs")
add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE} ${DOXYFILE_IN} ${DOXYFILE_OUT})
set_target_properties( Doxygen PROPERTIES FOLDER Docs)
set(FIND_DOXYGEN_OUTPUTS -Dbreathe_projects.TeleportVR=${DOXYGEN_OUTPUT_DIR}/xml)

# Now a Doxygen project to build the Unreal C# docs.
set(TELEPORT_DOXYGEN_UNREAL_INPUT_DIR ${TELEPORT_ROOT_DIR}/firstparty/teleport-unreal)
set(DOXYGEN_UNREAL_INPUT_DIR ${TELEPORT_DOXYGEN_UNREAL_INPUT_DIR})
set(DOXYGEN_UNREAL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen_unreal)
set(DOXYGEN_UNREAL_INDEX_FILE ${DOXYGEN_UNREAL_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_UNREAL_IN ${CMAKE_CURRENT_SOURCE_DIR}/DoxyfileUnreal.in)
set(DOXYFILE_UNREAL_OUT ${CMAKE_CURRENT_BINARY_DIR}/DoxyfileUnreal)
#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_UNREAL_IN} ${DOXYFILE_UNREAL_OUT} @ONLY)
file(MAKE_DIRECTORY ${DOXYGEN_UNREAL_OUTPUT_DIR}) #Doxygen won't create this for us

message("add_custom_target(DoxygenUnreal ALL
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNREAL_OUT}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS ${DOXYGEN_UNREAL_INDEX_FILE}
					MAIN_DEPENDENCY ${DOXYGEN_UNREAL_INDEX_FILE}
					COMMENT Generating Unreal docs
					SOURCES ${DOXYFILE_UNREAL_IN})")
add_custom_target(DoxygenUnreal ALL
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNREAL_OUT}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS ${DOXYGEN_UNREAL_INDEX_FILE}
					MAIN_DEPENDENCY ${DOXYGEN_UNREAL_INDEX_FILE}
					COMMENT "Generating Unreal docs"
					SOURCES ${DOXYFILE_UNREAL_IN})
set_target_properties( DoxygenUnreal PROPERTIES FOLDER Docs)
set(FIND_DOXYGEN_OUTPUTS ${FIND_DOXYGEN_OUTPUTS} -Dbreathe_projects.TeleportUnreal=${DOXYGEN_UNREAL_OUTPUT_DIR}/xml)

# Now a Doxygen project to build the Unity C# docs.
set(TELEPORT_DOXYGEN_UNITY_INPUT_DIR ${TELEPORT_ROOT_DIR}/firstparty/teleport-unity)
set(DOXYGEN_UNITY_INPUT_DIR ${TELEPORT_DOXYGEN_UNITY_INPUT_DIR})
set(DOXYGEN_UNITY_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen_unity)
set(DOXYGEN_UNITY_INDEX_FILE ${DOXYGEN_UNITY_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_UNITY_IN ${CMAKE_CURRENT_SOURCE_DIR}/DoxyfileUnity.in)
set(DOXYFILE_UNITY_OUT ${CMAKE_CURRENT_BINARY_DIR}/DoxyfileUnity)
#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_UNITY_IN} ${DOXYFILE_UNITY_OUT} @ONLY)
file(MAKE_DIRECTORY ${DOXYGEN_UNITY_OUTPUT_DIR}) #Doxygen won't create this for us

message("add_custom_target(DoxygenUnity ALL
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNITY_OUT}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS ${DOXYGEN_UNITY_INDEX_FILE}
					MAIN_DEPENDENCY ${DOXYGEN_UNITY_INDEX_FILE}
					COMMENT Generating Unity docs
					SOURCES ${DOXYFILE_UNITY_IN})")
add_custom_target(DoxygenUnity ALL
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNITY_OUT}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS ${DOXYGEN_UNITY_INDEX_FILE}
					MAIN_DEPENDENCY ${DOXYGEN_UNITY_INDEX_FILE}
					COMMENT "Generating Unity docs"
					SOURCES ${DOXYFILE_UNITY_IN})
set_target_properties( DoxygenUnity PROPERTIES FOLDER Docs)
set(FIND_DOXYGEN_OUTPUTS ${FIND_DOXYGEN_OUTPUTS} -Dbreathe_projects.TeleportUnity=${DOXYGEN_UNITY_OUTPUT_DIR}/xml)


set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../firstparty/Platform/CMake" ${CMAKE_MODULE_PATH})
find_package(Sphinx REQUIRED)
set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sphinx) #Sphinx won't create this for us
file(GLOB RST_ROOT_FILES "*.rst" )
file(GLOB PY_FILES "*.py" ) 
file(GLOB THEME_FILES "teleport_sphinx_theme/*.html" ) 
file(GLOB THEME_CSS_FILES "teleport_sphinx_theme/static/css/*.css" ) 

file(GLOB RST_REF_FILES "reference/*.rst" )
file(GLOB RST_PROTOCOL_FILES "protocol/*.rst" )
file(GLOB RST_PROTOCOL_SERVICE_FILES "protocol/service/*.rst" )
file(GLOB RST_UNITY_FILES "unity/*.rst" )
file(GLOB RST_CLIENT_FILES "client/*.rst" )
file(GLOB RST_UNREAL_FILES "unreal/*.rst" )
file(GLOB RST_NODEJS_FILES "nodejs/*.rst" )

#message("teleportrenderer_HEADER_DIRS ${teleportrenderer_HEADER_DIRS}")
source_group("Theme" FILES ${THEME_FILES} )
source_group("Theme/Css" FILES ${THEME_CSS_FILES} )
source_group("Documentation" FILES ${RST_ROOT_FILES} )
source_group("Documentation/Reference" FILES ${RST_REF_FILES} )
source_group("Documentation/Protocol" FILES ${RST_PROTOCOL_FILES} )
source_group("Documentation/Protocol/Service" FILES ${RST_PROTOCOL_SERVICE_FILES} )
source_group("Documentation/Unity" FILES ${RST_UNITY_FILES} )
source_group("Documentation/Client" FILES ${RST_CLIENT_FILES} ) 
source_group("Documentation/Unreal" FILES ${RST_UNREAL_FILES} )
source_group("Documentation/NodeJS" FILES ${RST_NODEJS_FILES} )
set(RST_FILES ${RST_ROOT_FILES} ${RST_REF_FILES} ${RST_PROTOCOL_FILES} ${RST_PROTOCOL_SERVICE_FILES} ${RST_UNITY_FILES} ${RST_CLIENT_FILES} ${RST_UNREAL_FILES} ${RST_NODEJS_FILES})
set(ALL_RST_FILES ${RST_FILES} ${THEME_FILES} ${THEME_CSS_FILES} ${PY_FILES})
add_custom_target(Sphinx ALL
					COMMAND
					${SPHINX_EXECUTABLE} -b html
					# Tell Breathe where to find the Doxygen output
					${FIND_DOXYGEN_OUTPUTS}
					${SPHINX_SOURCE} ${SPHINX_BUILD}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS ${ALL_RST_FILES} 
					# Other docs files you want to track should go here (or in some variable)
					${CMAKE_CURRENT_SOURCE_DIR}/index.rst 
					${DOXYGEN_INDEX_FILE}
					MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
					COMMENT "Generating documentation at ${CMAKE_CURRENT_BINARY_DIR}/sphinx/index.html"
					SOURCES ${ALL_RST_FILES})
set_target_properties( Sphinx PROPERTIES FOLDER Docs)

set_target_properties( Sphinx PROPERTIES 
		VS_DEBUGGER_COMMAND "C:/Program Files/Google/Chrome/Application/chrome.exe" 
		)

set_target_properties( Sphinx PROPERTIES 
		VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_CURRENT_BINARY_DIR}/sphinx/index.html")

if(NOT "${TELEPORT_DOXYGEN_UNITY_INPUT_DIR}" STREQUAL "")
	#add_dependencies(Sphinx DoxygenUnity Doxygen)
endif()

# see http://www.html-tidy.org/
set(TELEPORT_DOCS_TIDY_EXECUTABLE "C:/Program Files/tidy 5.8.0/bin/tidy.exe" CACHE FILEPATH "path to tidy.exe for htmltidy")

if(TELEPORT_DOCS_TIDY_EXECUTABLE)
	add_custom_target(Tidy ALL
					COMMAND 
					${CMAKE_CURRENT_SOURCE_DIR}/tidy_files.bat "${TELEPORT_DOCS_TIDY_EXECUTABLE}" "${SPHINX_BUILD}"
					MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/tidy_files.bat"
					COMMENT "${CMAKE_CURRENT_SOURCE_DIR}/tidy_files.bat" "${TELEPORT_DOCS_TIDY_EXECUTABLE}" "${SPHINX_BUILD}"
					SOURCES tidy_files.bat ${ALL_HTML_FILES})
	add_dependencies(Tidy Sphinx)
set_target_properties( Tidy PROPERTIES FOLDER Docs)
endif() 

if(TELEPORT_DOCS_VALIDATOR_EXECUTABLE)
	# see https://validator.github.io/validator/
	#C:/Program Files/vnu-runtime-image/bin/vnu.bat
	set(TELEPORT_DOCS_VALIDATOR_EXECUTABLE "" CACHE FILEPATH "path to vnu.bat")

	file(GLOB_RECURSE ALL_HTML_FILES ${SPHINX_BUILD}/*.html)
	if(NOT "${TELEPORT_DOCS_VALIDATOR_EXECUTABLE}" STREQUAL "")
		add_custom_target(Validator ALL
						COMMAND
						${TELEPORT_DOCS_VALIDATOR_EXECUTABLE} --format gnu --asciiquotes --skip-non-html ${ALL_HTML_FILES}
						WORKING_DIRECTORY ${SPHINX_BUILD}
						COMMENT "Validating documentation nu validator"
						SOURCES ${ALL_HTML_FILES})
		add_dependencies(Validator Tidy)
	set_target_properties( Validator PROPERTIES FOLDER Docs)
	endif()
endif()