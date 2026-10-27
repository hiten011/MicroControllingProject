# Cross-platform project: keep the build directory local to each developer.
# CMakeCache.txt contains absolute paths and cannot be shared across machines.
# If configuration reports paths from another machine, delete build/ and configure again.
# Do not commit generated build outputs to the shared Git repository.

# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(Practical5)

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(PROJECT_NAME Practical5)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Firmware binary is named after the main program (main.elf), not the project label.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME main)

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
  "Src/robot_control.c"
  "Src/G474RE_Drivers/uart.c"
  "Src/Kobuki_Drivers/kobuki.c"
  "Src/ADC/adc.c"
  "Src/Ultrasonic/ultrasonic.c"
  "Src/stm_helper/my_stm_helper.c"
  "Src/FSM/fsm.c"
  "Src/FSM_state/fsm_states.c"
  "Src/FSM_state/states/action_fault.c"
  "Src/FSM_state/states/state_idle.c"
  "Src/FSM_state/states/state_search.c"
  "Src/FSM_state/states/state_detect.c"
  "Src/FSM_state/states/state_spin.c"
  "Src/FSM_state/states/state_align.c"
  "Src/FSM_state/states/state_orient.c"
  "Src/FSM_state/states/state_pathfind_drive.c"
  "Src/FSM_state/states/state_pathfind_wall_hit.c"
  "Src/FSM_state/states/state_pathfind_ledge_hit.c"
  "Src/FSM_state/states/state_pathfind_realign.c"
  "Src/FSM_state/states/state_pathfind_front_cliff_turn.c"
  "Src/FSM_state/states/state_pathfind_front_cliff_stop.c"
  "Src/FSM_state/states/state_storage_turn.c"
  "Src/FSM_state/states/state_storage_drive_in.c"
  "Src/FSM_state/states/state_storage_deposit.c"
  "Src/FSM_state/states/state_fault.c"
  "Src/Helper/helper.c"
)

# Add include directories
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
  Src/FSM
  Src/FSM_state
  Src/FSM_state/states
  Src/Helper
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  # Add additional defined symbols here
  STM32G4xx
  USE_HAL_DRIVER
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE 
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  # Add additional linker libraries here
)