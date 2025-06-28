set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

find_program(CMAKE_C_COMPILER NAMES riscv64-linux-gnu-gcc)
find_program(CMAKE_CXX_COMPILER NAMES riscv64-linux-gnu-g++)

if(NOT CMAKE_C_COMPILER)
	message(FATAL_ERROR "Failed to find CMAKE_C_COMPILER.")
endif()

if(NOT CMAKE_CXX_COMPILER)
	message(FATAL_ERROR "Failed to find CMAKE_CXX_COMPILER.")
endif()
