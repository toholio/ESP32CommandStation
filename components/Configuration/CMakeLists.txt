set(COMPONENT_SRCS
    "ConfigurationManager.cpp"
    "LCCStackManager.cpp"
    "LCCWiFiManager.cpp"
)

set(COMPONENT_ADD_INCLUDEDIRS "include" )

set(COMPONENT_REQUIRES
    "DCCSignalGenerator"
    "Esp32HttpServer"
    "spiffs"
    "vfs"
    "fatfs"
    "OpenMRNLite"
)

register_component()

set_source_files_properties(ConfigurationManager.cpp PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough -Wno-ignored-qualifiers")
set_source_files_properties(LCCStackManager.cpp PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough -Wno-ignored-qualifiers")
set_source_files_properties(LCCWiFiManager.cpp PROPERTIES COMPILE_FLAGS "-Wno-implicit-fallthrough -Wno-ignored-qualifiers")