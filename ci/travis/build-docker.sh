#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

set -eu

# This script is supposed to run inside a Docker container, see
# ci/travis/build-linux.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="/v"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/colors.sh"

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

(cd "${PROJECT_ROOT}" ; ./ci/check-style.sh)

CMAKE_COMMAND="cmake"
if [ "${SCAN_BUILD}" = "yes" ]; then
  CMAKE_COMMAND="scan-build --use-cc=${CC} --use-c++=${CXX} cmake"
fi

ccache_command="$(command -v ccache)"

if [[ -z "${ccache_command}" ]]; then
  echo "The Travis builds cannot complete without ccache(1), exiting."
  exit 1
fi

echo
echo "${COLOR_YELLOW}Starting docker build $(date) with ${NCPU} cores${COLOR_RESET}"
echo

bootstrap_ccache="no"
if [[ "${NEEDS_CCACHE:-}" = "no" ]]; then
  bootstrap_ccache="no"
elif ${ccache_command} --show-stats | grep '^cache size' | grep -q '0.0 kB'; then
  echo "${COLOR_RED}"
  echo "The ccache is empty. The builds cannot finish in the time allocated by"
  echo "Travis without a warm cache. As a workaround, until #1800 is fixed,"
  echo "the cache is bootstrapped in two steps. First, only a subset of the"
  echo "system will be built, and the build will terminate with a failure."
  echo "The second build on the same branch will have at least the previously"
  echo "mentioned subset of the system in the build cache, and should be able"
  echo "to finish in the time allocated by Travis."
  echo "${COLOR_RESET}"
  bootstrap_ccache="yes"
fi

echo "${COLOR_YELLOW}Started CMake config at: $(date)${COLOR_RESET}"
echo "travis_fold:start:configure-cmake"
# Extra flags to pass to CMake based on our build configurations.
declare -a cmake_extra_flags
if [ "${BUILD_TESTING:-}" = "no" ]; then
  cmake_extra_flags+=( "-DBUILD_TESTING=OFF" )
fi

if [ "${TEST_INSTALL:-}" = "yes" ]; then
  cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package" )
fi

if [ "${SCAN_BUILD:-}" = "yes" ]; then
  cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER=package" "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF" )
fi

if [ "${USE_LIBCXX:-}" = "yes" ]; then
  cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_USE_LIBCXX=ON" )
fi

# We use parameter expansion for ${cmake_extra_flags} because set -u doesn't
# like empty arrays on older versions of Bash (which some of our builds use).
# The expression ${parameter+word} will expand word only if parameter is not
# unset. We also disable the shellcheck warning because we want ${CMAKE_FLAGS}
# to expand as separate arguments.
# shellcheck disable=SC2086
${CMAKE_COMMAND} \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    "${cmake_extra_flags[@]+"${cmake_extra_flags[@]}"}" \
    ${CMAKE_FLAGS:-} \
    -H. \
    -B"${BUILD_OUTPUT}"
echo
echo "travis_fold:end:configure-cmake"
echo "${COLOR_YELLOW}Finished CMake config at: $(date)${COLOR_RESET}"

# CMake can generate dependency graphs, which are useful to understand and
# troubleshoot dependencies.
if [[ "${CREATE_GRAPHVIZ:-}" = "yes" ]]; then
  ${CMAKE_COMMAND} \
      --graphviz="${BUILD_OUTPUT}/graphviz/google-cloud-cpp" \
      --build "${BUILD_OUTPUT}"
fi

# If scan-build is enabled, we need to manually compile the dependencies;
# otherwise, the static analyzer finds issues in them, and there is no way to
# ignore them.  When scan-build is not enabled, this is still useful because
# we can fold the output in Travis and make the log more interesting.
echo "${COLOR_YELLOW}Started dependency build at: $(date)${COLOR_RESET}"
echo "travis_fold:start:build-dependencies"
echo
cmake --build "${BUILD_OUTPUT}" \
    --target google-cloud-cpp-dependencies -- -j "${NCPU}"
echo
echo "travis_fold:end:build-dependencies"
echo "${COLOR_YELLOW}Finished dependency build at: $(date)${COLOR_RESET}"

if [[ "${bootstrap_ccache}" == "yes" ]]; then
  echo
  echo
  echo "${COLOR_RED}Aborting the build early to warm up the cache.${COLOR_RESET}"
  echo
  echo
  exit 1
fi

# If scan-build is enabled we build the smallest subset of things that is
# needed; otherwise, we pick errors from things we do not care about. With
# scan-build disabled we compile everything, to test the build as most
# developers will experience it.
echo "${COLOR_YELLOW}Started build at: $(date)${COLOR_RESET}"
${CMAKE_COMMAND} --build "${BUILD_OUTPUT}" -- -j "${NCPU}"
echo "${COLOR_YELLOW}Finished build at: $(date)${COLOR_RESET}"

# If ccache is enabled we want to zero out the statistics because otherwise
# Travis needs to rebuild the cache each time, and that slows down the build
# unnecessarily.
if [ -n "${ccache_command}" ]; then
  echo
  echo "${COLOR_YELLOW}Print and clearing ccache stats: $(date)${COLOR_RESET}"
  "${ccache_command}" --show-stats
  max_size="1Gi"
  if [ "${BUILD_TYPE}" = "Coverage" ]; then
    max_size="2.5Gi"
  fi
  "${ccache_command}" --zero-stats --cleanup --max-size="${max_size}"
fi

# Collect the output from the Clang static analyzer and provide instructions to
# the developers on how to do that locally.
if [[ "${SCAN_BUILD:-}" = "yes" ]]; then
  if [[ -n "$(ls -1d /tmp/scan-build-* 2>/dev/null)" ]]; then
    cp -r /tmp/scan-build-* /v/scan-cmake-out
  fi
  if [[ -r scan-cmake-out/index.html ]]; then
    cat <<_EOF_;

${COLOR_RED}
scan-build detected errors.  Please read the log for details. To
run scan-build locally and examine the HTML output install and configure Docker,
then run:

BUILD_NAME=scan-build ./ci/kokoro/docker/build.sh

The HTML output will be copied into the scan-cmake-out subdirectory.
${COLOR_RESET}
_EOF_
    exit 1
  else
    echo
    echo "${COLOR_GREEN}scan-build completed without errors.${COLOR_RESET}"
  fi
fi

if [ "${BUILD_TESTING:-}" = "yes" ]; then
  # Run the tests and output any failures.
  echo
  echo "${COLOR_YELLOW}Running unit and integration tests $(date)${COLOR_RESET}"
  echo
  (cd "${BUILD_OUTPUT}" && ctest --output-on-failure)

  # Run the integration tests. Not all projects have them, so just iterate over
  # the ones that do.
  for subdir in google/cloud google/cloud/bigtable google/cloud/storage; do
    echo
    echo "${COLOR_GREEN}Running integration tests for ${subdir}${COLOR_RESET}"
    (cd "${BUILD_OUTPUT}" && "${PROJECT_ROOT}/${subdir}/ci/run_integration_tests.sh")
  done
  echo
  echo "${COLOR_YELLOW}Completed unit and integration tests $(date)${COLOR_RESET}"
  echo
fi

# Test the install rule and that the installation works.
if [[ "${TEST_INSTALL:-}" = "yes" ]]; then
  echo
  echo "${COLOR_YELLOW}Testing install rule.${COLOR_RESET}"
  cmake --build "${BUILD_OUTPUT}" --target install || echo "FAILED"
  echo
  echo "${COLOR_YELLOW}Test installed libraries using cmake(1).${COLOR_RESET}"
  readonly TEST_INSTALL_DIR="${PROJECT_ROOT}/ci/test-install"
  readonly TEST_INSTALL_CMAKE_OUTPUT_DIR="${PROJECT_ROOT}/cmake-out/test-install-cmake"
  readonly TEST_INSTALL_MAKE_OUTPUT_DIR="${PROJECT_ROOT}/cmake-out/test-install-make"
  readonly TEST_INSTALL_SUBMODULE_OUTPUT_DIR="${PROJECT_ROOT}/cmake-out/test-install-submodule"
  cmake -H"${TEST_INSTALL_DIR}" -B"${TEST_INSTALL_CMAKE_OUTPUT_DIR}" -DCMAKE_CXX_COMPILER="${CXX}"
  cmake --build "${TEST_INSTALL_CMAKE_OUTPUT_DIR}"
  echo
  echo "${COLOR_YELLOW}Test installed libraries using make(1).${COLOR_RESET}"
  mkdir -p "${TEST_INSTALL_MAKE_OUTPUT_DIR}"
  make -C "${TEST_INSTALL_MAKE_OUTPUT_DIR}" -f"${TEST_INSTALL_DIR}/Makefile" VPATH="${TEST_INSTALL_DIR}" CXX="${CXX}"
  echo
  echo "${COLOR_YELLOW}Test installed libraries when used as a submodule.${COLOR_RESET}"
  mkdir -p "${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}/submodule/google-cloud-cpp"
  cp -r "${TEST_INSTALL_DIR}"/* "${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}"
  find . -mindepth 1 \( -path ./.git \
      -o -path ./third_party \
      -o -path './cmake-build-*' \
      -o -path ./build-output \
      -o -path ./.build \
      -o -path ./_build \
      -o -path ./cmake-out \
      -o -path . -type d \) -prune -o -exec cp --parents -r '{}' "${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}/submodule/google-cloud-cpp" \;
  cmake -H"${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}/submodule" -B"${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}" -DCMAKE_CXX_COMPILER="${CXX}"
  cmake --build "${TEST_INSTALL_SUBMODULE_OUTPUT_DIR}"

  # Checking the ABI requires installation, so this is the first opportunity to
  # run the check.
  (cd "${PROJECT_ROOT}" ; ./ci/check-abi.sh)

  # Also verify that the install directory does not get unexpected files or
  # directories installed.
  echo
  echo "${COLOR_YELLOW}Verify installed headers created only" \
      " expected directories.${COLOR_RESET}"
  cmake --build "${BUILD_OUTPUT}" --target install -- DESTDIR=/var/tmp/staging
  if comm -23 \
      <(find /var/tmp/staging/usr/local/include/google/cloud -type d | sort) \
      <(echo /var/tmp/staging/usr/local/include/google/cloud ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/bigtable ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/bigtable/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/firestore ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/spanner; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage/internal ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage/oauth2 ; \
        echo /var/tmp/staging/usr/local/include/google/cloud/storage/testing ; \
        /bin/true) | grep -q /var/tmp; then
      echo "${COLOR_YELLOW}Installed directories do not match expectation.${COLOR_RESET}"
      echo "${COLOR_RED}Found:"
      find /var/tmp/staging/usr/local/include/google/cloud -type d | sort
      echo "${COLOR_RESET}"
      /bin/false
   fi
fi

# If document generation is enabled, run it now.
if [ "${GENERATE_DOCS}" = "yes" ]; then
  echo
  echo "${COLOR_YELLOW}Generating Doxygen documentation at: $(date).${COLOR_RESET}"
  cmake --build "${BUILD_OUTPUT}" --target doxygen-docs
fi
