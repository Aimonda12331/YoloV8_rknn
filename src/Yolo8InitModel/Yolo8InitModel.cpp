#include <Yolo8InitModel/Yolo8InitModel.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

RKNNModel::RKNNModel() : ctx(0), modelData(nullptr), modelSize(0) {}

RKNNModel::~RKNNModel() {
    if (ctx != 0) {
        rknn_destroy(ctx);
    }
    if (modelData) {
        free(modelData);
    }
}

bool RKNNModel::loadModel(const std::string& modelPath) {
    FILE* fp = fopen(modelPath.c_str(), "rb");
    if (!fp) {
        std::cerr << "Failed to open RKNN model: " << modelPath << std::endl;
        return false;
    }

    fseek(fp, 0, SEEK_END);
    modelSize = ftell(fp);
    rewind(fp);

    modelData = (uint8_t*)malloc(modelSize);
    if (!modelData) {
        std::cerr << "Failed to allocate memory for model data" << std::endl;
        fclose(fp);
        return false;
    }

    fread(modelData, 1, modelSize, fp);
    fclose(fp);

    int ret = rknn_init(&ctx, modelData, modelSize, 0, nullptr);
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_init failed! Error code: " << ret << std::endl;
        return false;
    }

    std::cout << "RKNN model loaded successfully from: " << modelPath << std::endl;
    return true;
}

rknn_context RKNNModel::getContext() const {
    return ctx;
}
