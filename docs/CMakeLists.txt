find_package(Doxygen REQUIRED)

# Find all the public headers
get_target_property(pc_client_DIR pc_client SOURCE_DIR)
set(pc_client_HEADER_DIRS ${pc_client_DIR})
get_target_property(libavstream_DIR libavstream SOURCE_DIR)
set(libavstream_HEADER_DIRS ${libavstream_DIR}/src ${libavstream_DIR}/src/audio)
get_target_property(teleportclient_DIR TeleportClient SOURCE_DIR)
set(teleportclient_HEADER_DIRS ${teleportclient_DIR})
#message("teleportclient_HEADER_DIR ${teleportclient_HEADER_DIRS}")
get_target_property(teleportrenderer_DIR SimulCasterRenderer SOURCE_DIR)
set(teleportrenderer_HEADER_DIRS ${teleportrenderer_DIR}/src/api ${teleportrenderer_DIR}/src/crossplatform)
set(DOXYGEN_INPUT_DIR ${pc_client_HEADER_DIRS} ${libavstream_HEADER_DIRS} ${teleportclient_HEADER_DIRS} ${teleportrenderer_HEADER_DIRS})
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

add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})
set_target_properties( Doxygen PROPERTIES FOLDER Docs)

set(FIND_DOXYGEN_OUTPUTS -Dbreathe_projects.TeleportVR=${DOXYGEN_OUTPUT_DIR}/xml)
# Now a Doxygen project to build the Unity C# docs.
set(TELEPORT_DOXYGEN_UNITY_INPUT_DIR "" CACHE PATH "Location of the Teleport Unity plugin")
if(NOT "${TELEPORT_DOXYGEN_UNITY_INPUT_DIR}" STREQUAL "")
    #message("TELEPORT_DOXYGEN_UNITY_INPUT_DIR ${TELEPORT_DOXYGEN_UNITY_INPUT_DIR}")
    set(DOXYGEN_UNITY_INPUT_DIR ${TELEPORT_DOXYGEN_UNITY_INPUT_DIR})
    set(DOXYGEN_UNITY_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen_unity)
    set(DOXYGEN_UNITY_INDEX_FILE ${DOXYGEN_UNITY_OUTPUT_DIR}/xml/index.xml)
    set(DOXYFILE_UNITY_IN ${CMAKE_CURRENT_SOURCE_DIR}/DoxyfileUnity.in)
    set(DOXYFILE_UNITY_OUT ${CMAKE_CURRENT_BINARY_DIR}/DoxyfileUnity)

    #Replace variables inside @@ with the current values
    configure_file(${DOXYFILE_UNITY_IN} ${DOXYFILE_UNITY_OUT} @ONLY)

    file(MAKE_DIRECTORY ${DOXYGEN_UNITY_OUTPUT_DIR}) #Doxygen won't create this for us
    add_custom_command(OUTPUT ${DOXYGEN_UNITY_INDEX_FILE}
                       #DEPENDS ${PUBLIC_HEADERS}
                       COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_UNITY_OUT}
                       MAIN_DEPENDENCY ${DOXYFILE_UNITY_OUT} ${DOXYFILE_UNITY_IN}
                       COMMENT "Generating Unity docs")

    add_custom_target(DoxygenUnity ALL DEPENDS ${DOXYGEN_UNITY_INDEX_FILE})
    set_target_properties( DoxygenUnity PROPERTIES FOLDER Docs)
    set(FIND_DOXYGEN_OUTPUTS ${FIND_DOXYGEN_OUTPUTS} -Dbreathe_projects.TeleportUnity=${DOXYGEN_UNITY_OUTPUT_DIR}/xml)
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/firstparty/Platform/CMake" ${CMAKE_MODULE_PATH})
find_package(Sphinx REQUIRED)

set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)

file(GLOB RST_FILES 	"*.rst" )
source_group("Documentation" FILES ${RST_FILES} )
add_custom_target(Sphinx ALL
                  COMMAND
                  ${SPHINX_EXECUTABLE} -b html
                  # Tell Breathe where to find the Doxygen output
                  ${FIND_DOXYGEN_OUTPUTS}
                  ${SPHINX_SOURCE} ${SPHINX_BUILD}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS
                   # Other docs files you want to track should go here (or in some variable)
                   ${CMAKE_CURRENT_SOURCE_DIR}/index.rst ${RST_FILES}
                   ${DOXYGEN_INDEX_FILE}
                   MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
                  COMMENT "Generating documentation with Sphinx"
                  SOURCES ${RST_FILES})
set_target_properties( Sphinx PROPERTIES FOLDER Docs)


if(NOT "${TELEPORT_DOXYGEN_UNITY_INPUT_DIR}" STREQUAL "")
    add_dependencies(Sphinx DoxygenUnity Doxygen)
endif()

