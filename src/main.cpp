#include "engine.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        EngineConfig config;
        for (int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if (arg=="--validate-pbr") { PbrRenderer::validateAssets("models"); return EXIT_SUCCESS; }
            if (arg=="--legacy") config.pbr=false;
            else if (arg=="--frames" && i+1<argc) {
                config.frameLimit=std::stoi(argv[++i]);
                if (config.frameLimit<=0) throw std::runtime_error("--frames must be positive");
            } else if (arg!="--legacy") throw std::runtime_error("Usage: Engine [--legacy] [--frames N] [--validate-pbr]");
        }
        Engine engine(config);
        if (config.pbr) {
            auto damaged=engine.loadModel("models/DamagedHelmet/DamagedHelmet.gltf");
            auto flight=engine.loadModel("models/FlightHelmet/FlightHelmet.gltf");
            engine.createObject(damaged,engine.fitTransform(damaged,{-0.85f,1,0},1.5f));
            engine.createObject(flight,engine.fitTransform(flight,{0.85f,1,0},1.5f));
        }
        engine.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
