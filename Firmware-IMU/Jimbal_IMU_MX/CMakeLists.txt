cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Jimbal_IMU_MX)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# 링커 옵션 설정 부분에 추가
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -u _printf_float
)

# 1. 소스 파일 자동 검색 (일일이 등록하기 귀찮을 때 사용)
# GLOB_RECURSE는 "MyApp 폴더 안을 샅샅이 뒤져서 .c로 끝나는 모든 파일을 찾아라"라는 뜻입니다.
# SRC_FILES라는 변수에 그 파일들의 '전체 경로 목록'이 담기게 됩니다.
file(GLOB_RECURSE SRC_FILES "MyApp/*.c")


# 2. 실행 파일에 소스 추가 (컴파일 대상 지정)
# STM32는 하나의 거대한 실행 파일(.elf)을 만듭니다. 
# 여기에 CubeMX가 만든 기본 소스 외에, 우리가 만든 'SRC_FILES'를 포함시키겠다고 선언하는 겁니다.
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # ${SRC_FILES}는 위에서 찾은 ap.c, hw.c, mpu6050.c 등을 한꺼번에 리스트로 던져주는 역할입니다.
    ${SRC_FILES} 
)


# 3. 헤더 파일 경로 설정 (가장 중요! 별표 다섯 개)
# C언어에서 #include "ap.h"를 썼을 때, 컴퓨터는 이 파일이 어디 있는지 모릅니다.
# target_include_directories는 컴퓨터에게 "파일을 못 찾겠으면 이 폴더들을 뒤져봐"라고 지도를 그려주는 겁니다.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    
    # [왜 이렇게 세분화 하나요?]
    # 1. MyApp: 최상위 접근용
    # 2. MyApp/common: hw_def.h 같은 공통 설정 파일을 어디서든 이름만으로 불러오기 위해
    # 3. MyApp/hw/driver: mpu6050.h 같은 드라이버 헤더를 바로 인식시키기 위해
    
    MyApp
    MyApp/ap
    MyApp/ap/service
    MyApp/bsp
    MyApp/common      # 여기에 hw_def.h가 있으니 필수!
    MyApp/hw
    MyApp/hw/driver   # 여기에 mpu6050.h가 있으니 필수!
)

# # Add sources to executable
# target_sources(${CMAKE_PROJECT_NAME} PRIVATE
#     # Add user sources here
# )

# # Add include paths
# target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
#     # Add user defined include paths
# )


# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
