cmake_minimum_required(VERSION 3.0.0)
project(encoder VERSION 0.0.1)

add_compile_options(-Wall)

add_executable(encoder encoder_main.cpp encoder.cpp)
target_link_libraries(encoder gpiod)
