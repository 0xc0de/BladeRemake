set( PROJECT BladeOfDarkness )

file( GLOB Root *.h *.cpp )
file( GLOB ImGui imgui/*.h imgui/*.cpp )

source_group( "" FILES ${Root} )
source_group( "ImGui" FILES ${ImGui} )

set( SRCS
${Root}
${ImGui}
)

set( RESOURCES Angie.rc Icon1.ico )

set( SRCS ${SRCS} ${RESOURCES} )

setup_msvc_runtime_library()

if(WIN32)
add_executable( ${PROJECT} WIN32 ${SRCS} )
else()
add_executable( ${PROJECT} ${SRCS} )
endif()
target_link_libraries( ${PROJECT} ${AN_LIBS} ${AN_TOOLCHAIN_LIBS} )

set_property( TARGET ${PROJECT} PROPERTY POSITION_INDEPENDENT_CODE ON )

target_compile_definitions(${PROJECT} PUBLIC ${AN_COMPILER_DEFINES})
target_compile_options(${PROJECT} PUBLIC ${AN_COMPILER_FLAGS})

set_target_properties( ${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../Binary/Win64" )
set_target_properties( ${PROJECT} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/../Binary/Win64" )
