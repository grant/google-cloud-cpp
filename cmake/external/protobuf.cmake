# ~~~
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

include(ExternalProjectHelper)
find_package(Threads REQUIRED)
include(external/zlib)

if (NOT TARGET protobuf_project)
    # Give application developers a hook to configure the version and hash
    # downloaded from GitHub.
    set(GOOGLE_CLOUD_CPP_PROTOBUF_URL
        "https://github.com/google/protobuf/archive/v3.7.1.tar.gz")
    set(GOOGLE_CLOUD_CPP_PROTOBUF_SHA256
        "f1748989842b46fa208b2a6e4e2785133cfcc3e4d43c17fecb023733f0f5443f")

    set_external_project_build_parallel_level(PARALLEL)

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH "<INSTALL_DIR>/lib")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_INSTALL_RPATH
                   "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}")

    create_external_project_library_byproduct_list(protobuf_byproducts
                                                   "protobuf")

    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    set(GOOGLE_CLOUD_CPP_PREFIX_PATH "${CMAKE_PREFIX_PATH};<INSTALL_DIR>")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_PREFIX_PATH
                   "${GOOGLE_CLOUD_CPP_PREFIX_PATH}")

    include(ExternalProject)
    externalproject_add(
        protobuf_project
        DEPENDS zlib_project
        EXCLUDE_FROM_ALL ON
        PREFIX "${CMAKE_BINARY_DIR}/external/protobuf"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
        URL ${GOOGLE_CLOUD_CPP_PROTOBUF_URL}
        URL_HASH SHA256=${GOOGLE_CLOUD_CPP_PROTOBUF_SHA256}
        LIST_SEPARATOR |
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND}
            -G${CMAKE_GENERATOR}
            ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_PREFIX_PATH=${GOOGLE_CLOUD_CPP_PREFIX_PATH}
            -DCMAKE_INSTALL_RPATH=${GOOGLE_CLOUD_CPP_INSTALL_RPATH}
            # Protobuf installs using `CMAKE_INSTALL_LIBDIR`, as it should,
            # which expands to `lib` or `lib64`. But hard-codes RPATH to
            # `$ORIGIN/../lib`, so change the default `LIBDIR` to something
            # that works.
            # https://github.com/protocolbuffers/protobuf/pull/6204
            -DCMAKE_INSTALL_LIBDIR=lib
            -Dprotobuf_BUILD_TESTS=OFF
            -Dprotobuf_DEBUG_POSTFIX=
            -H<SOURCE_DIR>/cmake
            -B<BINARY_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND}
                      --build
                      <BINARY_DIR>
                      ${PARALLEL}
        BUILD_BYPRODUCTS ${protobuf_byproducts}
                         <INSTALL_DIR>/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON)

    if (TARGET google-cloud-cpp-dependencies)
        add_dependencies(google-cloud-cpp-dependencies protobuf_project)
    endif ()

    add_library(protobuf::libprotobuf INTERFACE IMPORTED)
    add_dependencies(protobuf::libprotobuf protobuf_project)
    set_library_properties_for_external_project(protobuf::libprotobuf protobuf
                                                ALWAYS_LIB)
    set_property(TARGET protobuf::libprotobuf
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES
                          protobuf::libprotobuf
                          ZLIB::ZLIB
                          Threads::Threads)
    add_executable(protobuf::protoc IMPORTED)
    set_property(
        TARGET protobuf::protoc
        PROPERTY
            IMPORTED_LOCATION
            "${PROJECT_BINARY_DIR}/external/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}"
        )
endif ()
