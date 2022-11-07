# SPDX-License-Identifier: GPL-2.0-or-later

set(EXPAT_EXTRA_ARGS
  -DEXPAT_BUILD_DOCS=OFF
  -DEXPAT_BUILD_EXAMPLES=OFF
  -DEXPAT_BUILD_TESTS=OFF
  -DEXPAT_BUILD_TOOLS=OFF
  -DEXPAT_SHARED_LIBS=OFF
)

ExternalProject_Add(external_expat
  URL file://${PACKAGE_DIR}/${EXPAT_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${EXPAT_HASH_TYPE}=${EXPAT_HASH}
  PREFIX ${BUILD_DIR}/expat
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/expat ${DEFAULT_CMAKE_FLAGS} ${EXPAT_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/expat
  SOURCE_SUBDIR expat
)
