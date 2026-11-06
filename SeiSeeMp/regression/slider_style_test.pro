QT += widgets svg testlib
CONFIG += c++11 console testcase
CONFIG -= app_bundle
TARGET = slider_style_test
TEMPLATE = app

INCLUDEPATH += ../../CustomWdgets
SOURCES += slider_style_test.cpp
RESOURCES += ../myrc.qrc
