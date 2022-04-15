llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )
file (GLOB SOURCES
   *.cpp
)
add_executable(svf-data-flow ${SOURCES})

target_link_libraries(svf-data-flow ${SVF_LIB} ${LLVMCudd} ${llvm_libs})
target_link_libraries(svf-data-flow ${Z3_LIBRARIES})
set_target_properties( svf-data-flow PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
