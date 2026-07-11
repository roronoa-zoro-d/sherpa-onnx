add_library(zeeo_kws SHARED
  core/kws_engine.cc
)

target_include_directories(zeeo_kws
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zeeo_kws
  PUBLIC
    sherpa-onnx-core
)

target_include_directories(zeeo_kws
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)

set_target_properties(zeeo_kws PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

if(SHERPA_ONNX_ENABLE_BINARY)
  add_executable(feed_pcm_demo src/feed_pcm_demo.cc)
  target_link_libraries(feed_pcm_demo PRIVATE zeeo_kws)
  set_target_properties(feed_pcm_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
endif()

install(TARGETS zeeo_kws DESTINATION lib)
install(DIRECTORY include/zeeo_kws DESTINATION include)
