# Plain Qt unit test project (no sailfishapp dependency), run via `%check` in the
# RPM spec and locally with `qmake tests/tests.pro && make check`.
TARGET = tst_driver
TEMPLATE = app
CONFIG += console c++14 testcase
CONFIG -= app_bundle
QT += testlib network positioning
QT -= gui

INCLUDEPATH += ../src

SOURCES += \
    tst_driver.cpp \
    ../src/FlexiblePolyline.cpp \
    ../src/HereUrls.cpp \
    ../src/ItineraryParser.cpp \
    ../src/NavGeo.cpp \
    ../src/Navigator.cpp \
    ../src/NavigatorTransit.cpp \
    ../src/AppSettings.cpp \
    ../src/HereClient.cpp

HEADERS += \
    ../src/FlexiblePolyline.h \
    ../src/HereUrls.h \
    ../src/ItineraryParser.h \
    ../src/NavGeo.h \
    ../src/Navigator.h \
    ../src/AppSettings.h \
    ../src/HereClient.h
