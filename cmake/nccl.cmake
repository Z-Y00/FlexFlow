set(NCCL_NAME nccl)
# set(NCCL_CUDA_ARCH "-gencode=arch=compute_${CUDA_ARCH},code=sm_${CUDA_ARCH}")
# message("NCCL_CUDA_ARCH: ${NCCL_CUDA_ARCH}")

ExternalProject_Add(${NCCL_NAME}
 SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/${NCCL_NAME}
 PREFIX ${CMAKE_BINARY_DIR}/deps/${NCCL_NAME}
 INSTALL_DIR ${CMAKE_BINARY_DIR}/deps/${NCCL_NAME}
 BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/deps/${NCCL_NAME}/lib/libnccl${LIBEXT}
 INSTALL_COMMAND ""
 CONFIGURE_COMMAND ""
 BUILD_COMMAND make src.build "NVCC_GENCODE=${CUDA_GENCODE}" "CUDA_HOME=${CUDA_TOOLKIT_ROOT_DIR}" "BUILDDIR=${CMAKE_BINARY_DIR}/deps/${NCCL_NAME}"
 BUILD_IN_SOURCE 1
)

ExternalProject_Get_Property(${NCCL_NAME} INSTALL_DIR)
message(STATUS "NCCL install dir: ${INSTALL_DIR}")
list(APPEND FLEXFLOW_INCLUDE_DIRS
  ${INSTALL_DIR}/include)
list(APPEND FLEXFLOW_EXT_LIBRARIES
  ${INSTALL_DIR}/lib/libnccl${LIBEXT})
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/deps/${NCCL_NAME}/lib/")
