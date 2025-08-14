#ifndef OPENGL_RTSP_PROCESS_H
#define OPENGL_RTSP_PROCESS_H

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif

    #include <iostream>
    #include <atomic>
    #include <thread>
    #include <mutex>
    #include <chrono>
    #include <fcntl.h>
    #include <unistd.h>

    #include <EGL/egl.h>
    #include <EGL/eglext.h>
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>

class OpenGL_DMABuf{
public:
    OpenGL_DMABuf();
    ~OpenGL_DMABuf();

    //Hàm bật tắt hiển thị
    void start();
    void stop();

    //Hàm đẩy frame mới vào queue (fd từ appsink callback)
    void pushFrame(int fd, int width, int height);
    void toggleRender();

private:
    std::chrono::steady_clock::time_point lastTime_{};
    int frameCount_ = 0;
    int lastFPS_ = 0;

    void renderLoop();
    void initEGL();
    void initGL();
    void renderDMABUF(int fd, int w, int h);
    void cleanup();

    std::atomic<bool> running_;
    std::atomic<bool> rendering_;
    
    std::thread renderThread_;

    int lastFD_ = -1;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
    std::mutex frameMutex_;

    // EGL/GL handle
    EGLDisplay eglDisplay_;
    EGLContext eglContext_;
    EGLSurface eglSurface_;
    GLuint shaderProgram_;

        // Function pointers for EGL extensions
    PFNEGLCREATEIMAGEKHRPROC p_eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC p_eglDestroyImageKHR;
};
#endif