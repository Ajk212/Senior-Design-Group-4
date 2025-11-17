file(GLOB_RECURSE srcs "main.cpp" "src/*.c" "src/*.cpp")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash driver esp_adc
                       INCLUDE_DIRS "./include" )