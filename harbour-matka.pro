# Matka - a WeGo-style trip planner for Sailfish OS, built on the HERE APIs.

TARGET = harbour-matka

CONFIG += sailfishapp c++14
QT += network positioning

VERSION = 0.8.0
DEFINES += APP_VERSION=\\\"$${VERSION}\\\"

SOURCES += \
    src/main.cpp \
    src/AppSettings.cpp \
    src/HereClient.cpp \
    src/HereUrls.cpp \
    src/FlexiblePolyline.cpp \
    src/ItineraryParser.cpp \
    src/TripPlanner.cpp \
    src/SearchController.cpp \
    src/NavGeo.cpp \
    src/Navigator.cpp \
    src/NavigatorTransit.cpp

HEADERS += \
    src/AppSettings.h \
    src/HereClient.h \
    src/HereUrls.h \
    src/FlexiblePolyline.h \
    src/ItineraryParser.h \
    src/TripPlanner.h \
    src/SearchController.h \
    src/NavGeo.h \
    src/Navigator.h

DISTFILES += \
    qml/harbour-matka.qml \
    qml/AppRoot.qml \
    rpm/harbour-matka.spec \
    harbour-matka.desktop

assets.files = assets
assets.path = /usr/share/$${TARGET}
INSTALLS += assets

fixtures.files = fixtures
fixtures.path = /usr/share/$${TARGET}
INSTALLS += fixtures

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/harbour-matka.ts
