ADD_EXECUTABLE(nugu_response_filter main.cc filter.cc)
TARGET_LINK_LIBRARIES(nugu_response_filter
	${pkgs_LDFLAGS}
	-L${CMAKE_BINARY_DIR}/src -lnugu)
ADD_DEPENDENCIES(nugu_response_filter libnugu)
INSTALL(TARGETS nugu_response_filter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
