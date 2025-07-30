#ifndef YOLO_INIT_MODEL_H
#define YOLO_INIT_MODEL_H
    
    #include <iostream>
    #include <string>
    #include <rknn_api.h>

class RKNNModel{
public: 
    RKNNModel();
    ~RKNNModel();

    bool loadModel(const std::string& modelPath);
    rknn_context getContext() const;

private:
    rknn_context ctx;
    uint8_t* modelData;
    size_t modelSize;

};

#endif