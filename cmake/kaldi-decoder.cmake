function(download_kaldi_decoder)
  include(FetchContent)

  # When kaldifst-src already exists (e.g. second cmake run), stage OpenFst next to it for offline nested openfst.cmake.
  set(_kfst_src "${FETCHCONTENT_BASE_DIR}/kaldifst-src")
  set(_of_can "openfst-1.8.5-2026-04-10.tar.gz")
  if(EXISTS "${_kfst_src}")
    foreach(_cand IN ITEMS
        "${SHERPA_ONNX_DEPS_DIR}/${_of_can}"
        "${CMAKE_SOURCE_DIR}/depends/${_of_can}"
        "${SHERPA_ONNX_DEPS_DIR}/openfst-kaldifst-1.8.5-2026-04-10.tar.gz"
        "${CMAKE_SOURCE_DIR}/depends/openfst-kaldifst-1.8.5-2026-04-10.tar.gz"
      )
      if(EXISTS "${_cand}")
        get_filename_component(_of_bn "${_cand}" NAME)
        file(COPY "${_cand}" DESTINATION "${_kfst_src}")
        if(NOT _of_bn STREQUAL "${_of_can}")
          file(RENAME "${_kfst_src}/${_of_bn}" "${_kfst_src}/${_of_can}")
        endif()
        message(STATUS "Offline: staged ${_of_can} into kaldifst-src for nested OpenFst")
        break()
      endif()
    endforeach()
  endif()

  set(kaldi_decoder_URL  "https://github.com/k2-fsa/kaldi-decoder/archive/refs/tags/v0.3.0.tar.gz")
  set(kaldi_decoder_HASH "SHA256=b9f34cfb4fd3b1344100eead79ef4d37aa15962274b9e3056de345021f76a1b0")

  set(KALDI_DECODER_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
  set(KALDI_DECODER_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(KALDIFST_BUILD_PYTHON OFF CACHE BOOL "" FORCE)

  # If you don't have access to the Internet,
  # please pre-download kaldi-decoder (first tried: ${SHERPA_ONNX_DEPS_DIR}/… i.e. <source>/depends/…)
  set(possible_file_locations
    ${CMAKE_SOURCE_DIR}/depends/kaldi-decoder-0.3.0.tar.gz
    $ENV{HOME}/Downloads/kaldi-decoder-0.3.0.tar.gz
    ${CMAKE_SOURCE_DIR}/kaldi-decoder-0.3.0.tar.gz
    ${CMAKE_BINARY_DIR}/kaldi-decoder-0.3.0.tar.gz
    /tmp/kaldi-decoder-0.3.0.tar.gz
    /star-fj/fangjun/download/github/kaldi-decoder-0.3.0.tar.gz
  )
  sherpa_onnx_prepend_depdir(possible_file_locations "kaldi-decoder-0.3.0.tar.gz")

  foreach(f IN LISTS possible_file_locations)
    if(EXISTS ${f})
      set(kaldi_decoder_URL  "${f}")
      file(TO_CMAKE_PATH "${kaldi_decoder_URL}" kaldi_decoder_URL)
      message(STATUS "Found local downloaded kaldi-decoder: ${kaldi_decoder_URL}")
      break()
    endif()
  endforeach()

  FetchContent_Declare(kaldi_decoder
    URL
      ${kaldi_decoder_URL}
    URL_HASH          ${kaldi_decoder_HASH}
  )

  FetchContent_GetProperties(kaldi_decoder)
  if(NOT kaldi_decoder_POPULATED)
    message(STATUS "Downloading kaldi-decoder from ${kaldi_decoder_URL}")
    FetchContent_Populate(kaldi_decoder)
  endif()
  message(STATUS "kaldi-decoder is downloaded to ${kaldi_decoder_SOURCE_DIR}")
  message(STATUS "kaldi-decoder's binary dir is ${kaldi_decoder_BINARY_DIR}")

  # Nested FetchContent inside kaldi_decoder looks for tarballs next to kaldi_decoder-src.
  foreach(_nest IN ITEMS eigen-3.4.0.tar.gz kaldifst-1.8.0.tar.gz)
    set(_ok FALSE)
    foreach(_base IN ITEMS "${SHERPA_ONNX_DEPS_DIR}" "${CMAKE_SOURCE_DIR}/depends")
      if(EXISTS "${_base}/${_nest}")
        file(COPY "${_base}/${_nest}" DESTINATION "${kaldi_decoder_SOURCE_DIR}")
        message(STATUS "Offline: staged ${_nest} into kaldi-decoder tree (${_base})")
        set(_ok TRUE)
        break()
      endif()
    endforeach()
    if(NOT _ok)
      message(WARNING "Missing ${_nest} under depends/ — nested CMake may download from the network")
    endif()
  endforeach()

  include_directories(${kaldi_decoder_SOURCE_DIR})

  if(BUILD_SHARED_LIBS)
    set(_build_shared_libs_bak ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
  endif()

  list(APPEND CMAKE_MODULE_PATH ${kaldi_decoder_SOURCE_DIR}/cmake)

  add_subdirectory(${kaldi_decoder_SOURCE_DIR} ${kaldi_decoder_BINARY_DIR} EXCLUDE_FROM_ALL)

  if(_build_shared_libs_bak)
    set_target_properties(
        kaldi-decoder-core
      PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
    )
    set(BUILD_SHARED_LIBS ON)
  endif()

  if(WIN32 AND MSVC)
    target_compile_options(kaldi-decoder-core PUBLIC
      /wd4018
      /wd4291
    )
  endif()

  set_target_properties(kaldifst_core PROPERTIES OUTPUT_NAME "sherpa-onnx-kaldifst-core")

  target_include_directories(kaldi-decoder-core
    INTERFACE
      ${kaldi-decoder_SOURCE_DIR}/
  )
  if(NOT BUILD_SHARED_LIBS)
    install(TARGETS
      kaldi-decoder-core
      kaldifst_core
      fst
      fstfar
    DESTINATION lib)
  endif()
endfunction()

download_kaldi_decoder()

