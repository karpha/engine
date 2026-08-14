#pragma once
#include <tiny_obj_loader.h>

#include <vector>
#include <stdexcept>
#include <unordered_map>

class Buffer;

class LoadModel{
public:
    LoadModel(Buffer* buff ){
        pBuffer = buff;
        loadModel();
    }

    LoadModel(const LoadModel&) = delete;
    LoadModel& operator=(const LoadModel&) = delete;
    void loadModel();
private:
    const std::string MODEL_PATH = "models/viking_room.obj";
    Buffer* pBuffer;
};