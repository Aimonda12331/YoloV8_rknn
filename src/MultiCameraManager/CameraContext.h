#ifndef CAMERA_CONTEXT_H
#define CAMERA_CONTEXT_H

    #include <iostream>
    #include <string>
    #include <thread>

    //
    class rtsp_reader;

    struct CameraContext{
        std::string url;
        std::unique_ptr<rtsp_reader> reader;
        std::thread th;
};

#endif