cmake_minimum_required (VERSION 3.16)

project ("Compiler")

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)
include(CheckCXXCompilerFlag)

# پیدا کردن پکیج‌های LLVM
find_package(LLVM REQUIRED CONFIG)
message("Found LLVM ${LLVM_PACKAGE_VERSION}, build type ${LLVM_BUILD_TYPE}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(DetermineGCCCompatible)
include(ChooseMSVCCRT)

add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

# تعریف کتابخانه‌های مورد نیاز LLVM
llvm_map_components_to_libnames(llvm_libs Core irreader bitreader support target mc)

# تنظیمات استاندارد
if(LLVM_COMPILER_IS_GCC_COMPATIBLE)
    if(NOT LLVM_ENABLE_RTTI)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
    endif()
    if(NOT LLVM_ENABLE_EH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
    endif()
endif()

# رفتن به پوشه سورس
add_subdirectory ("src")