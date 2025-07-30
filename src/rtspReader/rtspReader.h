#pragma once
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <gst/allocators/gstdmabuf.h>

class RTSPReader {
public:
    RTSPReader(const std::string& rtsp_URL);
    ~RTSPReader();

    bool open();
    void stop();
    void readStream();

    bool getCurrentFrame(cv::Mat& outFrame);
    int getCurrentDmaFd();

private:
    void readThreadFunc();
    cv::Mat gstSampleToMat(GstSample* sample);

    std::string url;
    bool running;
    std::thread readThread;
    std::mutex frameMutex;

    GstElement* pipeline;
    GstElement* appsink;
    cv::Mat currentFrame;
};
