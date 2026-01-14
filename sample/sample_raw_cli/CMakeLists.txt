find_package(Libevent)

if(ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUV_LINK_NAME OR Libevent_FOUND)

  get_filename_component(SAMPLE_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
  set(SAMPLE_NAME "hiredis-happ-${SAMPLE_NAME}")

  include_directories(${CMAKE_CURRENT_LIST_DIR})

  aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_FILES)

  if(ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUV_LINK_NAME)
    add_compiler_define(HIREDIS_HAPP_ENABLE_LIBUV=1)
    list(APPEND SAMPLE_EXT_LIBS ${ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUV_LINK_NAME})
    if(WIN32)
      list(APPEND SAMPLE_EXT_LIBS psapi iphlpapi userenv ws2_32)
    endif()
  elseif(Libevent_FOUND)
    add_compiler_define(HIREDIS_HAPP_ENABLE_LIBEVENT=1)
    set(SAMPLE_EXT_LIBS ${Libevent_LIBRARIES})
    include_directories(${Libevent_INCLUDE_DIRS})
  endif()

  if(NOT MSVC)
    set(SAMPLE_EXT_LIBS ${SAMPLE_EXT_LIBS} pthread)
  endif()

  add_executable(${SAMPLE_NAME} ${SAMPLE_SRC_FILES})
  target_link_libraries(${SAMPLE_NAME} hiredis-happ ${SAMPLE_EXT_LIBS} ${COMPILER_OPTION_EXTERN_CXX_LIBS})

endif()
