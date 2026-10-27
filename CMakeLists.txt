# Cross-platform project: keep the build directory local to each developer.
# CMakeCache.txt contains absolute paths and cannot be shared across machines.
# If configuration reports paths from another machine, delete build/ and configure again.
# Do not commit generated build outputs to the shared Git repository.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

project(Practical5)

enable_language(C ASM)

set(PROJECT_NAME Practical5)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME main)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

target_sources(${PROJECT_NAME} PRIVATE
  "Src/robot_control.c"
  "Src/G474RE_Drivers/uart.c"
  "Src/Kobuki_Drivers/kobuki.c"
  "Src/ADC/adc.c"
  "Src/Ultrasonic/ultrasonic.c"
  "Src/stm_helper/my_stm_helper.c"
  "Src/Helper/helper.c"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  Inc/drivers/CMSIS/Core/Include
  Inc/drivers/BSP/STM32G4xx_Nucleo
  Inc/drivers/STM32G4xx_HAL_Driver/Inc
  Inc/drivers/Core/Inc
  Inc/drivers/CMSIS/Device/ST/STM32G4xx/Include
  Src/G474RE_Drivers
  Src/Kobuki_Drivers
  Src/ADC
  Src/Ultrasonic
  Src/stm_helper
  Src/Helper
)

target_compile_options(${PROJECT_NAME} PRIVATE
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32G4xx
  USE_HAL_DRIVER
)

target_link_options(${PROJECT_NAME} PRIVATE 
)

target_link_directories(${PROJECT_NAME} PRIVATE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
)
