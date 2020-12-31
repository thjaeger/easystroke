cmake_minimum_required(VERSION 3.17)
project(easy-gestured)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig)
find_package(X11)
find_package(Boost 1.71 REQUIRED serialization)

pkg_check_modules(GTKMM gtkmm-3.0)

# Hint: Use ls -1 and remove non-project files
set(SOURCE_FILES 
    actiondb.cc
    actiondb.h
    actions.cc
    actions.h
    composite.cc
    composite.h
    desktop.cc
    gesture.cc
    gesture.h
    grabber.cc
    grabber.h
    handler.cc
    handler.h
    main.cc
    main.h
    prefdb.cc
    prefdb.h
    stats.cc
    stroke.c
    stroke.h
    trace.h
    util.h
    var.h
    win.cc
    win.h
)

link_directories(${GTKMM_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS})

include_directories(.
    ${GTKMM_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR} ${X11_XTest_INCLUDE_PATH} ${X11_Xinput_INCLUDE_PATH} ${X11_Xfixes_INCLUDE_PATH}
    ${Boost_INCLUDE_DIRS}
)
add_executable(easy-gestured ${SOURCE_FILES})

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    ${GTKMM_LIBRARIES}
    ${X11_LIBRARIES} ${X11_XTest_LIB} ${X11_Xinput_LIB} ${X11_Xfixes_LIB}
    ${Boost_LIBRARIES}
)
