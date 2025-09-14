file(GLOB_RECURSE driver_srcs "driver/*.c")

idf_component_register(SRCS ${driver_srcs} "main.c" "lvgl_api.c"
                    INCLUDE_DIRS "." "driver")

set(COMPONENT_REQUIRES lvgl)

# SPIFFS 镜像配置
spiffs_create_partition_image(
    storage 
    spiffs
    FLASH_IN_PROJECT
)