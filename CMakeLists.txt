cmake_minimum_required(VERSION 3.10)
project(YoloV8_RKNN LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DEGL_EGLEXT_PROTOTYPES)

# ==== Tìm các thư viện cần thiết ====
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.14)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_ALLOC REQUIRED gstreamer-allocators-1.0)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESv2 REQUIRED glesv2)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GBM REQUIRED gbm)

find_library(RGA_LIB rga PATHS /usr/lib/aarch64-linux-gnu /usr/local/lib)
if(NOT RGA_LIB)
    message(WARNING "⚠️ RGA library not found - NV12 conversion may not work")
endif()

# ==== Khai báo file nguồn ====
set(SRC
    main.cpp
    src/rtspProcess/mpp_rtspProcess.cpp
    src/nv12_converter_all_formats/nv12_converter_all_formats.cpp
    src/MultiCameraManager/multi_thread.cpp
    # src/ThreadPool/ThreadPool.cpp
    # src/Yolo8InitModel/Yolo8InitModel.cpp
)

# ==== Copy shader vào thư mục build ====
file(GLOB SHADERS src/nv12_converter/shaders/*.vert src/nv12_converter/shaders/*.frag)
foreach(SHADER ${SHADERS})
    configure_file(${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/ COPYONLY)
endforeach()

# ==== Tạo executable ====
add_executable(yolo8 ${SRC})

# ==== Include directories ====
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
    src
    src/rtspProcess
    src/nv12_converter
    src/MultiCameraManager
    src/ThreadPool
    src/Yolo8InitModel
    /usr/include/rockchip
    /usr/include/libdrm
    /usr/include/rga
    ${CMAKE_SOURCE_DIR}/lib/Crow/include
)

# ==== Link thư viện ====
target_link_libraries(yolo8 PRIVATE
    OpenSSL::SSL 
    OpenSSL::Crypto 
    pthread
    rknnrt
    rknn_api
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
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
    rga
)

# ==== Đặt thư mục build output ====
set_target_properties(yolo8 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

message(STATUS "🎉 Build cấu hình hoàn tất.")