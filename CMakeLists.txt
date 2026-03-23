cmake_minimum_required(VERSION 4.1)
project(design)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        include
        /opt/homebrew/include
        /usr/local/include
)

link_directories(
        /opt/homebrew/lib
        /usr/local/lib
)

find_library(CARBON_LIB Carbon)
find_library(APP_SERVICES_LIB ApplicationServices)
find_package(CURL REQUIRED)

add_executable(design src/main.cpp
        include/CollectingDataBadUSB.h
        src/CollectingDataBadUSB.cpp
        src/DisconnectKeyboard.cpp
        include/DisconnectKeyboard.h
        include/UsbUtils.h
        src/UsbUtils.cpp
        src/CollectingDataHuman.cpp
        include/BadUSBDetector.h
        src/BadUSBDetector.cpp
        include/GeminiAnalyzer.h
        src/GeminiAnalyzer.cpp
        include/GeminiAnalyzer.h
        src/GeminiAnalyzer.cpp
)

target_link_libraries(design
        ${CARBON_LIB}
        ${APP_SERVICES_LIB}
        ${CURL_LIBRARIES}
        pcap
        xgboost
        "-framework ApplicationServices"
        "-framework CoreFoundation"
)