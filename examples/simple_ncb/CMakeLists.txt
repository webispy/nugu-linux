ADD_EXECUTABLE(nugu_simple_ncb
    main_ncb.c
    ncb_helper.c
    fake_pcm.c
    fake_player.c
    fake_recorder.c)
TARGET_LINK_LIBRARIES(nugu_simple_ncb ${pkgs_LDFLAGS} -ldl)
ADD_DEPENDENCIES(nugu_simple_ncb libnugu)
INSTALL(TARGETS nugu_simple_ncb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
