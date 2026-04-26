# Prepend ${SHERPA_ONNX_DEPS_DIR}/<filename> so manual archives in ./depends/ are found
# before network download (same filenames as upstream docs / CMAKE_SOURCE_DIR examples).
macro(sherpa_onnx_prepend_depdir list_name filename)
  if(SHERPA_ONNX_DEPS_DIR)
    list(INSERT ${list_name} 0 "${SHERPA_ONNX_DEPS_DIR}/${filename}")
  endif()
endmacro()
