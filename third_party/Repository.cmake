if(NOT Libuv_FOUND AND NOT Libevent_FOUND)
  project_third_party_include_port("libuv/libuv.cmake")
endif()

project_third_party_include_port("ssl/port.cmake")
project_third_party_include_port("redis/hiredis.cmake")
