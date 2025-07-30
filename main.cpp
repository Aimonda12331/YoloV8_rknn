#include <iostream>
#include <rtspReader/rtspReader.h>
#include <Yolo8InitModel/Yolo8InitModel.h>

std::string yolov8_model_path = "./rknn_model/yolov8.rknn";

void RTSP_begin(){
                          
    std::string rtsp_URL = "rtsp://103.147.186.175:8554/9L02DA3PAJ39B2F";
    // std::string rtsp_URL = "rtsp://user03:abcd1234@113.177.126.84:8202";  // RTSP camera
    RTSPReader reader(rtsp_URL);

    if (!reader.open()) {
        std::cerr << "Lỗi kết nối RTSP!\n";
        return; // <-- KHÔNG trả về giá trị
    }
    reader.readStream();  // Vào vòng lặp đọc và hiển thị
    reader.stop();

    std::cout << "Ket thuc chuong trình" << std::endl;
}

void init_model(){
    RKNNModel model;
    if(!model.loadModel(yolov8_model_path)){
        std::cerr << "Tai yolov8 that bai!!!" << std::endl;
        return;
    }
    rknn_context ctx = model.getContext();

    // TODO: rknn_inputs_set, rknn_run, postprocess...
}

int main() {
    RTSP_begin();
    init_model();
return 0; 
}
