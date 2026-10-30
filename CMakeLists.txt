# Qt 6 build of Driver: Android (the APK) and desktop (for development).
# The Sailfish OS package is built with harbour-driver.pro instead.
cmake_minimum_required(VERSION 3.21)
project(driver VERSION 0.8.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Network Positioning Quick QuickControls2 Location)

# MapLibre Native Qt (prebuilt): per-ABI on Android.
if(ANDROID AND DEFINED ENV{QMapLibre_Android_DIR})
    set(QMapLibre_DIR "$ENV{QMapLibre_Android_DIR}/${ANDROID_ABI}/lib/cmake/QMapLibre")
endif()
# The prebuilt desktop package needs ICU 56, so desktop builds default to no
# map (the UI shows its "map unavailable" card) and are for layout work only.
if(ANDROID)
    set(DRIVER_MAPLIBRE_DEFAULT ON)
else()
    set(DRIVER_MAPLIBRE_DEFAULT OFF)
endif()
option(DRIVER_MAPLIBRE "Link MapLibre Native Qt" ${DRIVER_MAPLIBRE_DEFAULT})
if(DRIVER_MAPLIBRE)
    find_package(QMapLibre COMPONENTS Location REQUIRED)
endif()

# The QML tree for Qt 6 is generated from the Sailfish one (see tools/gen_qt6_qml.py).
set(GEN_QML ${CMAKE_CURRENT_BINARY_DIR}/qml)
file(GLOB_RECURSE QML_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/qml/*.qml ${CMAKE_SOURCE_DIR}/qml/*.js
     ${CMAKE_SOURCE_DIR}/qml/*/qmldir ${CMAKE_SOURCE_DIR}/android/qml/*.qml ${CMAKE_SOURCE_DIR}/android/qml/*/*.qml)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_qt6_qml.py ${CMAKE_SOURCE_DIR} ${GEN_QML}
                RESULT_VARIABLE GEN_RESULT)
if(NOT GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "QML generation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${QML_SOURCES})

qt_add_executable(driver
    src/main_qt6.cpp
    src/AppTools.cpp src/AppTools.h
    src/AppSettings.cpp src/AppSettings.h
    src/HereClient.cpp src/HereClient.h
    src/HereUrls.cpp src/HereUrls.h
    src/FlexiblePolyline.cpp src/FlexiblePolyline.h
    src/ItineraryParser.cpp src/ItineraryParser.h
    src/TripPlanner.cpp src/TripPlanner.h
    src/SearchController.cpp src/SearchController.h
    src/NavGeo.cpp src/NavGeo.h
    src/Navigator.cpp src/NavigatorTransit.cpp src/Navigator.h
    src/RoadAlerts.cpp src/RoadAlerts.h
    src/SignPacks.cpp src/SignPacks.h
    src/SpeedLimitStore.cpp src/SpeedLimitStore.h
    src/VoiceClips.cpp src/VoiceClips.h)
target_compile_definitions(driver PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_include_directories(driver PRIVATE src)
target_link_libraries(driver PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Positioning Qt6::Quick
                      Qt6::QuickControls2 Qt6::Location)
if(DRIVER_MAPLIBRE)
    target_link_libraries(driver PRIVATE QMapLibre::Location)
    qmaplibre_location_setup_plugins(driver)
endif()

# QML, fonts and fixtures go into the binary as resources.
file(GLOB_RECURSE QML_FILES RELATIVE ${GEN_QML} ${GEN_QML}/*)
set(QML_RES "")
foreach(f ${QML_FILES})
    list(APPEND QML_RES ${GEN_QML}/${f})
    set_source_files_properties(${GEN_QML}/${f} PROPERTIES QT_RESOURCE_ALIAS ${f})
endforeach()
qt_add_resources(driver "qml_files" PREFIX "/qml" BASE ${GEN_QML} FILES ${QML_RES})
file(GLOB_RECURSE ASSET_FILES RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/assets/fonts/*)
file(GLOB_RECURSE FIXTURE_FILES RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/fixtures/*)
set(OTHER_RES "")
foreach(f ${ASSET_FILES} ${FIXTURE_FILES})
    list(APPEND OTHER_RES ${CMAKE_SOURCE_DIR}/${f})
    set_source_files_properties(${CMAKE_SOURCE_DIR}/${f} PROPERTIES QT_RESOURCE_ALIAS ${f})
endforeach()
qt_add_resources(driver "data_files" PREFIX "/" BASE ${CMAKE_SOURCE_DIR} FILES ${OTHER_RES})

# Qt's TLS on Android needs OpenSSL shipped with the app (KDAB/android_openssl);
# without it neither HERE nor the map tiles can be fetched over https.
if(ANDROID)
    if(NOT DEFINED ENV{ANDROID_OPENSSL_DIR})
        message(FATAL_ERROR "Set ANDROID_OPENSSL_DIR to a checkout of github.com/KDAB/android_openssl")
    endif()
    include($ENV{ANDROID_OPENSSL_DIR}/android_openssl.cmake)
    add_android_openssl_libraries(driver)
endif()

if(ANDROID)
    set_target_properties(driver PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
        QT_ANDROID_PACKAGE_NAME io.github.defecador.driver
        QT_ANDROID_COMPILE_SDK_VERSION 34
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
        QT_ANDROID_VERSION_CODE 803)
endif()
