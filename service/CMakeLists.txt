cmake_minimum_required(VERSION 3.16)
project(easy-gestured)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig)
find_package(X11)

pkg_check_modules(GTKMM gtkmm-3.0)

# Hint: Use ls -1 and remove non-project files
set(SOURCE_FILES 
    actiondb.cc
    actiondb.h
    actions/action.cpp
    actions/action.h
    actions/modAction.cpp
    actions/modAction.h
    actions/modifiers.cpp
    actions/modifiers.h
    appxcontext.h
    composite.cc
    composite.h
    desktop.cc
    gesture.cc
    gesture.h
    globals.h
    grabber.cc
    grabber.h
    handler.cc
    handler.h
    log.cpp
    log.h
    main.cc
    prefdb.cc
    prefdb.h
    stroke.cpp
    stroke.h
    trace.cpp
    trace.h
    util.h
    var.h
        xserverproxy.cpp
        xserverproxy.h)

link_directories(${GTKMM_LIBRARY_DIRS})

include_directories(.
    ${GTKMM_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR} ${X11_XTest_INCLUDE_PATH} ${X11_Xinput_INCLUDE_PATH} ${X11_Xfixes_INCLUDE_PATH}
)
add_executable(easy-gestured ${SOURCE_FILES})

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    ${GTKMM_LIBRARIES}
    ${X11_LIBRARIES} ${X11_XTest_LIB} ${X11_Xinput_LIB} ${X11_Xfixes_LIB}
)
