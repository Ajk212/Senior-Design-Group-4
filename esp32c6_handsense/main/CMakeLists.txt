file(GLOB_RECURSE srcs "main.c" "src/*.c")

idf_component_register(SRCS "${srcs}"
                       REQUIRES bt nvs_flash driver esp_adc
                       INCLUDE_DIRS "./include" )