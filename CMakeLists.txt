cmake_minimum_required(VERSION 3.21)
project(SciCalc991 VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(calccore STATIC
  src/core/value.cpp
  src/core/lexer.cpp
  src/core/parser.cpp
  src/core/evaluator.cpp
  src/core/numeric.cpp
  src/core/format.cpp
  src/core/constants.cpp
  src/core/stats.cpp
)
target_include_directories(calccore PUBLIC src)
target_link_libraries(calccore PUBLIC Qt6::Core)

set(SCICALC_GUI_SOURCES
  src/main.cpp
  src/gui/mathview.cpp
  src/gui/keypad.cpp
  src/gui/mainwindow.cpp
  src/gui/modepage.cpp
  src/gui/calcpage.cpp
  src/gui/statpage.cpp
  src/gui/distpage.cpp
  src/gui/sheetpage.cpp
  src/gui/tablepage.cpp
  src/gui/equationpage.cpp
  src/gui/inequalitypage.cpp
  src/gui/dialogs.cpp
  src/gui/theme.cpp
  src/gui/richtext.cpp
  src/gui/customizedialog.cpp
)

if(IOS)
  # qt_add_executable pulls in the static iOS platform plugin and generates the app bundle.
  qt_add_executable(scicalc991 MACOSX_BUNDLE ${SCICALC_GUI_SOURCES})
  set(SCICALC_BUNDLE_ID "io.github.moondotgz.scicalc991" CACHE STRING "iOS bundle identifier")
  set_target_properties(scicalc991 PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "SciCalc 991"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${SCICALC_BUNDLE_ID}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SCICALC_BUNDLE_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  )
else()
  add_executable(scicalc991 ${SCICALC_GUI_SOURCES})
endif()
target_link_libraries(scicalc991 PRIVATE calccore Qt6::Widgets)

if(NOT IOS)
  enable_testing()
  add_executable(test_core tests/test_core.cpp)
  target_link_libraries(test_core PRIVATE calccore)
  add_test(NAME core COMMAND test_core)
endif()
