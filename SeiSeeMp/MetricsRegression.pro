include(SeiSeeMp.pro)
TARGET = metrics_regression
QT += testlib
CONFIG += console testcase
SOURCES -= main.cpp
SOURCES += ../tests/upstream_metrics_regression.cpp

win32-msvc {
    QMAKE_CXXFLAGS += /Zi
    QMAKE_LFLAGS += /DEBUG
}
