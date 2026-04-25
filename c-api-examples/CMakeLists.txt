# KWS-only C API demos (dev-kws branch)
include_directories(${PROJECT_SOURCE_DIR})

add_executable(kws-c-api kws-c-api.c)
target_link_libraries(kws-c-api sherpa-onnx-c-api)

add_executable(keywords-spotter-buffered-tokens-keywords-c-api
               keywords-spotter-buffered-tokens-keywords-c-api.c)
target_link_libraries(keywords-spotter-buffered-tokens-keywords-c-api sherpa-onnx-c-api)
