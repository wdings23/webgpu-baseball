file(GLOB SOURCES "*.c" "*.h") # Or other source file extensions
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})