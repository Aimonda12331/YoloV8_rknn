#include "rtspProcess/openGL_rtspProcess.h"
#include <chrono>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

    #define EGL_CHECK(x) do { x; } while(0)

OpenGL_DMABuf::OpenGL_DMABuf() 
    : running_(false), rendering_(true), lastFD_(-1), lastWidth_(0), lastHeight_(0),
      eglDisplay_(nullptr), eglContext_(nullptr), eglSurface_(nullptr),   
      shaderProgram_(0) {
}

OpenGL_DMABuf::~OpenGL_DMABuf(){
    stop();
}

void OpenGL_DMABuf::stop() {
    if (!running_) return;
    running_ = false;

    if (renderThread_.joinable())
        renderThread_.join();

    cleanup();
}

void OpenGL_DMABuf::start(){
    if(running_) return;
    running_      = true;
    renderThread_ = std::thread(&OpenGL_DMABuf::renderLoop, this);
}

void OpenGL_DMABuf::pushFrame(int fd, int width, int height){
{
    std::lock_guard<std::mutex> lk(frameMutex_);
    lastFD_     = fd;
    lastWidth_  = width;
    lastHeight_ = height;
}
        // ==== Đếm FPS ====
    frameCount_++;
    auto now = std::chrono::steady_clock::now();

     // Lần đầu tiên gọi: khởi tạo mốc thời gian
    if (lastTime_.time_since_epoch().count() == 0) {
        lastTime_ = now;
        return;
    }

    // Nếu đã qua 1 giây → in FPS
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime_).count();
    if (elapsed >= 1000) {
        lastFPS_ = frameCount_;
        frameCount_ = 0;
        lastTime_ = now;

        std::cout << "[INFO] Render DMABUF fd=" << lastFD_
                  << " size=" << lastWidth_ << "x" << lastHeight_
                  << " FPS= " << lastFPS_ << std::endl;
    }
}


void OpenGL_DMABuf::toggleRender() {
    rendering_ = !rendering_;
    std::cout << "[INFO] Render " << (rendering_ ? "ON" : "OFF") << std::endl;
}

void OpenGL_DMABuf::renderLoop() {
    initEGL();
    initGL();

    while (running_) {
         if (!rendering_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int fd, w , h;
        {
            std::lock_guard<std::mutex> lk(frameMutex_);
            fd = lastFD_;
            w = lastWidth_;
            h = lastHeight_;
        }

        if (fd >= 0) {
            renderDMABUF(fd, w, h);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }

    cleanup();
}

void OpenGL_DMABuf::initEGL() {
    std::cout << "[INFO] Init EGL GBM/DRM" << std::endl;

    int drmFd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drmFd < 0) {
        std::cerr << "[ERROR] Cannot open /dev/dri/card0" << std::endl;
        return;
    }

    gbm_device* gbm = gbm_create_device(drmFd);
    gbm_surface* surface = gbm_surface_create(
        gbm, 1280, 720,  // tạm thời 720p
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
    );

    eglDisplay_ = eglGetDisplay((EGLNativeDisplayType)gbm);
    eglInitialize(eglDisplay_, nullptr, nullptr);

    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay_, cfgAttrs, &config, 1, &numConfigs);

    eglSurface_ = eglCreateWindowSurface(eglDisplay_, config,
                                         (EGLNativeWindowType)surface, nullptr);

    EGLint ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglContext_ = eglCreateContext(eglDisplay_, config, EGL_NO_CONTEXT, ctxAttrs);

    eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_);
}

void OpenGL_DMABuf::initGL() {
    std::cout << "[INFO] Init OpenGL shader (placeholder)" << std::endl;
    // TODO: Compile shader NV12->RGB và setup quad
    // Hiện tại sẽ clear màu cho dễ test
    glClearColor(0.0f, 0.2f, 0.5f, 1.0f);
}

void OpenGL_DMABuf::renderDMABUF(int fd, int w, int h) {
    //std::cout << "[INFO] Render DMABUF fd=" << fd << " size=" << w << "x" << h << std::endl;
    // TODO: Import DMABUF bằng EGL_EXT_image_dma_buf_import rồi glDraw
    // 1. Clear background
    glClear(GL_COLOR_BUFFER_BIT);

    // 2. TODO: Import DMABUF -> EGLImage -> GL_TEXTURE_2D
    // EGLint attribs[] = {
    //     EGL_WIDTH, w,
    //     EGL_HEIGHT, h,
    //     EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_NV12,
    //     EGL_DMA_BUF_PLANE0_FD_EXT, fd,
    //     EGL_NONE
    // };
    // EGLImageKHR image = eglCreateImageKHR(...);
    // glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    // 3. TODO: vẽ quad với shader NV12->RGB

    // 4. SwapBuffers để hiển thị
    eglSwapBuffers(eglDisplay_, eglSurface_);


}

void OpenGL_DMABuf::cleanup() {
    std::cout << "[INFO] Cleanup OpenGL/EGL" << std::endl;
    
    if (eglDisplay_) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglSurface_) eglDestroySurface(eglDisplay_, eglSurface_);
        if (eglContext_) eglDestroyContext(eglDisplay_, eglContext_);
        eglTerminate(eglDisplay_);
    }
    eglDisplay_ = nullptr;
    eglSurface_ = nullptr;
    eglContext_ = nullptr;
}
