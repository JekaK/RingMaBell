cmake_minimum_required(VERSION 3.16)

project(RingMaBell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ringmabell
    src/AirAlertMonitor.cpp
    src/AudioPlayer.cpp
    src/Config.cpp
    src/Csv.cpp
    src/HttpClient.cpp
    src/InstanceLock.cpp
    src/Logger.cpp
    src/Main.cpp
    src/Schedule.cpp
    src/Scheduler.cpp
    src/StateStore.cpp
    src/Utils.cpp
)

target_include_directories(ringmabell PRIVATE src)

if(WIN32)
    target_link_libraries(ringmabell PRIVATE winmm winhttp)
endif()

install(TARGETS ringmabell RUNTIME DESTINATION bin)
install(DIRECTORY config sounds DESTINATION .)
