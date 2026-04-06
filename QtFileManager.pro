QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    MainWindow.h

RESOURCES += \
    resources.qrc

win32: RC_FILE += app.rc
