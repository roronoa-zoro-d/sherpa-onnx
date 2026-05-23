# Prepend ${SHERPA_ONNX_DEPS_DIR}/<filename> (default: ${CMAKE_SOURCE_DIR}/depends/<filename>)
# so archives copied for offline builds are found before network download.
macro(sherpa_onnx_prepend_depdir list_name filename)
  if(SHERPA_ONNX_DEPS_DIR)
    list(INSERT ${list_name} 0 "${SHERPA_ONNX_DEPS_DIR}/${filename}")
  endif()
endmacro()
