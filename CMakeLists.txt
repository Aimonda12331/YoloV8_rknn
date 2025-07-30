cmake_minimum_required(VERSION 3.10)

project(YoloV8_RKNN LANGUAGES C CXX)
message(STATUS "PROJECT: ${PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "USING C++17")

# -------------------- Tìm thư viện --------------------
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV_INCLUDE_DIRS = ${OpenCV_INCLUDE_DIRS}")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.14)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_ALLOC REQUIRED gstreamer-allocators-1.0)  # <== THÊM DÒNG NÀY

message(STATUS "Đã tìm thấy GStreamer modules")

# -------------------- Nguồn mã --------------------
set(SOURCES
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${PROJECT_SOURCE_DIR}/src/rtspReader/rtspReader.cpp
    ${PROJECT_SOURCE_DIR}/src/ThreadPool/ThreadPool.cpp
    ${PROJECT_SOURCE_DIR}/src/Yolo8InitModel/Yolo8InitModel.cpp
)

# -------------------- Tạo executable --------------------
add_executable(yolo8 ${SOURCES})
message(STATUS "Executable: yolo8")

# -------------------- MPP paths --------------------
set(MPP_INCLUDE_DIR /usr/include/rockchip)

# -------------------- Bao gồm đường dẫn header --------------------
target_include_directories(yolo8 PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_ALLOC_INCLUDE_DIRS}             # <== THÊM DÒNG NÀY

    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/rknn_model
)

# -------------------- Liên kết thư viện --------------------
target_link_libraries(yolo8 PRIVATE
    pthread
    rknn_api
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_ALLOC_LIBRARIES}                # <== THÊM DÒNG NÀY
    ${OpenCV_LIBS}
    rockchip_mpp
)
