cmake_minimum_required(VERSION 3.10)
project(YoloV8_RKNN LANGUAGES C CXX)

message(STATUS "PROJECT: ${PROJECT_NAME}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DEGL_EGLEXT_PROTOTYPES)

# -------------------- Thư viện cần tìm --------------------
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)

#find_package(X11 REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.14)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_ALLOC REQUIRED gstreamer-allocators-1.0)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESv2 REQUIRED glesv2)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GBM REQUIRED gbm)

# RGA library for hardware color conversion
find_library(RGA_LIB rga PATHS /usr/lib/aarch64-linux-gnu /usr/local/lib)
if(RGA_LIB)
    message(STATUS "✅ Found RGA library: ${RGA_LIB}")
else()
    message(WARNING "⚠️ RGA library not found - NV12 conversion may not work")
endif()

message(STATUS "✅ Tìm thấy tất cả các thư viện cần thiết.")

# -------------------- File nguồn --------------------
set(SOURCES
    ${PROJECT_SOURCE_DIR}/main.cpp

    ${PROJECT_SOURCE_DIR}/src/rtspProcess/mpp_rtspProcess.cpp
    ${PROJECT_SOURCE_DIR}/src/rtspProcess/openGL_rtspProcess.cpp

    ${PROJECT_SOURCE_DIR}/src/nv12_converter/nv12_converter.cpp

    # Uncomment nếu bạn dùng các thành phần sau:
    #${PROJECT_SOURCE_DIR}/src/ThreadPool/ThreadPool.cpp
    #${PROJECT_SOURCE_DIR}/src/Yolo8InitModel/Yolo8InitModel.cpp
)

# -------------------- Shader (copy vào thư mục build) --------------------
file(GLOB SHADERS
    ${PROJECT_SOURCE_DIR}/src/nv12_converter/shaders/*.vert
    ${PROJECT_SOURCE_DIR}/src/nv12_converter/shaders/*.frag
)

# Tự động copy shader vào thư mục build để dễ dùng
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    configure_file(${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME} COPYONLY)
endforeach()

# -------------------- Executable --------------------
add_executable(yolo8 ${SOURCES})
message(STATUS "Executable target: yolo8")

# -------------------- Include path --------------------
set(MPP_INCLUDE_DIR /usr/include/rockchip)

target_include_directories(yolo8 PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_ALLOC_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
    ${GBM_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}

    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/rtspProcess
    ${PROJECT_SOURCE_DIR}/src/nv12_converter
    ${PROJECT_SOURCE_DIR}/src/nv12_converter/shaders
    ${PROJECT_SOURCE_DIR}/src/ThreadPool
    ${PROJECT_SOURCE_DIR}/src/Yolo8InitModel

    ${MPP_INCLUDE_DIR}
    /usr/include/libdrm
    /usr/include/rga
)

# -------------------- Link thư viện --------------------
link_directories(/usr/lib/aarch64-linux-gnu)
target_link_libraries(yolo8 PRIVATE
    pthread
    rknnrt
    rknn_api
    ${OpenCV_LIBS}

    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_ALLOC_LIBRARIES}

    rockchip_mpp

    ${EGL_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLESv2_LIBRARIES}
    EGL
    GLESv2
    OpenGL

    drm
    gbm
    ${DRM_LIBRARIES}
    ${GBM_LIBRARIES}
    ${X11_LIBRARIES}
    
    # RGA library for NV12 conversion
    rga
)

# -------------------- Thư mục build riêng biệt --------------------
set_target_properties(yolo8 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

message(STATUS "🎉 Build cấu hình hoàn tất.")
