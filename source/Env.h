#pragma once

#include <string>
#include <fstream>
#include <cstdlib>

#if defined(__APPLE__) || defined(__linux__)
    #include <dlfcn.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif


namespace radiyx {

class Env
{
    public:
        static std::string GetPluginDir()
        {
        #if defined(__APPLE__) || defined(__linux__)
            Dl_info info;
            if (dladdr((void*)&GetPluginDir, &info)) {
                std::string fullPath = info.dli_fname;
                return fullPath.substr(0, fullPath.find_last_of('/'));
            }
            return ".";
        #elif defined(_WIN32)
            char path[MAX_PATH];
            HMODULE hModule = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetPluginDir, &hModule);
            GetModuleFileNameA(hModule, path, MAX_PATH);
            std::string fullPath(path);
            return fullPath.substr(0, fullPath.find_last_of("\\/"));
        #else
            return ".";
        #endif
        }

        static std::string GetRealProjectDir()
        {
            std::string pluginDir = GetPluginDir();

        #if defined(__APPLE__)
            const std::string suffix = "/Contents/MacOS";
            size_t pos = pluginDir.rfind(suffix);
            if (pos != std::string::npos) {
                std::string vst3Bundle = pluginDir.substr(0, pos);
                size_t buildPos = vst3Bundle.rfind("/build");
                if (buildPos != std::string::npos)
                    return vst3Bundle.substr(0, buildPos);
                return vst3Bundle;
            }
            return pluginDir;
        #elif defined(__linux__) || defined(_WIN32)
            // Ожидаем путь типа: /.../build/.../SineSynth.vst3 или \...\\build\\...\\SineSynth.vst3
            size_t buildPos = pluginDir.find("/build");
            if (buildPos == std::string::npos)
                buildPos = pluginDir.find("\\build");

            if (buildPos != std::string::npos)
                return pluginDir.substr(0, buildPos);

            return pluginDir;
        #else
            return pluginDir;
        #endif
        }

        static void LoadDotEnv()
        {
            std::string path = GetRealProjectDir() + "/.env";
            
            std::ifstream envFile(path);
            if (!envFile.is_open()) return;

            std::string line;
            while (std::getline(envFile, line))
            {
                auto eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;

                std::string key = line.substr(0, eqPos);
                std::string val = line.substr(eqPos + 1);
                setenv(key.c_str(), val.c_str(), 1);
            }
        }
};

} // namespace radiyx
