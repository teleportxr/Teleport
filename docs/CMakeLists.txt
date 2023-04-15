find_package(Doxygen REQUIRED)

# Find all the public headersc
set(pc_client_HEADER_DIRS ${CMAKE_SOURCE_DIR}/pc_client)
set(AV ${CMAKE_SOURCE_DIR}/libavstream/include/libavstream)
set(libavstream_HEADER_DIRS ${AV} ${AV}/audio ${AV}/decoders ${AV}/encoders ${AV}/geometry ${AV}/network ${AV}/platforms ${AV}/stream ${AV}/surfaces)

set(teleportclient_HEADER_DIRS ${CMAKE_SOURCE_DIR}/TeleportClient)
set(teleportcore_HEADER_DIRS ${CMAKE_SOURCE_DIR}/TeleportCore)

set(teleportrenderer_DIR	${CMAKE_SOURCE_DIR}/ClientRender)
set(teleportrenderer_HEADER_DIRS ${teleportrenderer_DIR} ${teleportrenderer_DIR}/NodeComponents ${teleportrenderer_DIR}/AVParser)

set(teleportserver_HEADER_DIRS ${CMAKE_SOURCE_DIR}/TeleportServer ${CMAKE_SOURCE_DIR}/TeleportServer/UnityPlugin)
set(DOXYGEN_INPUT_DIR ${pc_client_HEADER_DIRS} ${libavstream_HEADER_DIRS} ${teleportcore_HEADER_DIRS} ${teleportclient_HEADER_DIRS} ${teleportrenderer_HEADER_DIRS} ${teleportserver_HEADER_DIRS})
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
add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE} ${DOXYFILE_IN})
set_target_properties( Doxygen PROPERTIES FOLDER Docs)
set(FIND_DOXYGEN_OUTPUTS -Dbreathe_projects.TeleportVR=${DOXYGEN_OUTPUT_DIR}/xml)

# Now a Doxygen project to build the Unity C# docs.
set(TELEPORT_DOXYGEN_UNITY_INPUT_DIR ${CMAKE_SOURCE_DIR}/firstparty/teleport-unity)
set(DOXYGEN_UNITY_INPUT_DIR ${TELEPORT_DOXYGEN_UNITY_INPUT_DIR})
set(DOXYGEN_UNITY_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen_unity)
set(DOXYGEN_UNITY_INDEX_FILE ${DOXYGEN_UNITY_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_UNITY_IN ${CMAKE_CURRENT_SOURCE_DIR}/DoxyfileUnity.in)
set(DOXYFILE_UNITY_OUT ${CMAKE_CURRENT_BINARY_DIR}/DoxyfileUnity)
#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_UNITY_IN} ${DOXYFILE_UNITY_OUT} @ONLY)
file(MAKE_DIRECTORY ${DOXYGEN_UNITY_OUTPUT_DIR}) #Doxygen won't create this for us
#add_custom_command(OUTPUT ${DOXYGEN_UNITY_INDEX_FILE}
					#DEPENDS ${PUBLIC_HEADERS}
			#		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNITY_OUT}
			#		 MAIN_DEPENDENCY ${DOXYFILE_UNITY_OUT} ${DOXYFILE_UNITY_IN}
			#		COMMENT "Generating Unity docs")
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


set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/firstparty/Platform/CMake" ${CMAKE_MODULE_PATH})
find_package(Sphinx REQUIRED)
set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
file(GLOB RST_FILES "*.rst" ) 
file(GLOB PY_FILES "*.py" ) 
file(GLOB THEME_FILES "teleport_sphinx_theme/*.html" ) 
file(GLOB THEME_CSS_FILES "teleport_sphinx_theme/static/css/*.css" ) 
message("THEME_CSS_FILES ${THEME_CSS_FILES}")
file(GLOB RST_REF_FILES "reference/*.rst" )
file(GLOB RST_PROTOCOL_FILES "protocol/*.rst" )
file(GLOB RST_PROTOCOL_SERVICE_FILES "protocol/service/*.rst" )
file(GLOB RST_UNITY_FILES "unity/*.rst" )
file(GLOB RST_CLIENT_FILES "client/*.rst" )

message("teleportrenderer_HEADER_DIRS ${teleportrenderer_HEADER_DIRS}")
source_group("Theme" FILES ${THEME_FILES} )
source_group("Theme/Css" FILES ${THEME_CSS_FILES} )
source_group("Documentation" FILES ${RST_FILES} )
source_group("Documentation/Reference" FILES ${RST_REF_FILES} )
source_group("Documentation/Protocol" FILES ${RST_PROTOCOL_FILES} )
source_group("Documentation/Protocol/Service" FILES ${RST_PROTOCOL_SERVICE_FILES} )
source_group("Documentation/Unity" FILES ${RST_UNITY_FILES} )
source_group("Documentation/Client" FILES ${RST_CLIENT_FILES} )
set(ALL_RST_FILES ${RST_FILES} ${RST_REF_FILES} ${RST_PROTOCOL_FILES} ${RST_PROTOCOL_SERVICE_FILES} ${RST_UNITY_FILES} ${RST_CLIENT_FILES} ${THEME_FILES} ${THEME_CSS_FILES} ${PY_FILES})
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
					COMMENT "Generating documentation with Sphinx"
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

# see https://validator.github.io/validator/
set(TELEPORT_DOCS_VALIDATOR_EXECUTABLE "C:/Program Files/vnu-runtime-image/bin/vnu.bat" CACHE FILEPATH "path to vnu.bat")

set(ALL_HTML_FILES ${SPHINX_BUILD})
if(TELEPORT_DOCS_VALIDATOR_EXECUTABLE)
	add_custom_target(Validator ALL
					COMMAND
					${TELEPORT_DOCS_VALIDATOR_EXECUTABLE} --format gnu --asciiquotes --skip-non-html ${ALL_HTML_FILES}
					WORKING_DIRECTORY ${SPHINX_BUILD}
					COMMENT "Validating documentation nu validator"
					SOURCES ${ALL_HTML_FILES})
	add_dependencies(Validator Tidy)
set_target_properties( Validator PROPERTIES FOLDER Docs)
endif()