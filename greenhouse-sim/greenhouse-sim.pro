QT += widgets
TARGET = greenhouse-sim
TEMPLATE = app
CONFIG += c++17
SOURCES += \
    main.cpp \
    app_window.cpp \
    sim_launch.cpp \
    greenhouse_view.cpp \
    pipeline_view.cpp \
    draw_utils.cpp
HEADERS += \
    types.h \
    sim_launch.h \
    greenhouse_view.h \
    pipeline_view.h \
    draw_utils.h
