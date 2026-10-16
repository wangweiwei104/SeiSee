#-------------------------------------------------
#
# Project created by QtCreator 2013-10-14T17:36:40
#
#-------------------------------------------------

QT       += core gui svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SeiSeeMp
TEMPLATE = app
CONFIG += warn_off
CONFIG += c++11

SOURCES += main.cpp\
    ../CustomWdgets/customcontrols.cpp \
    edithdrdialog.cpp \
        mainwindow.cpp \
    procparmdialog.cpp \
    aboutdialog.cpp \
    axisdialog.cpp \
    exportdpimenustyle.cpp \
    workthread.cpp \
    saveasdialog.cpp \
    diffdialog.cpp \
    metricsdialog.cpp

HEADERS  += mainwindow.h \
    ../CustomWdgets/customcontrols.h \
    edithdrdialog.h \
    procparmdialog.h \
    aboutdialog.h \
    axisdialog.h \
    exportdpimenustyle.h \
    workthread.h \
    saveasdialog.h \
    diffdialog.h \
    metricsdialog.h

FORMS    += mainwindow.ui \
    edithdrdialog.ui \
    procparmdialog.ui \
    aboutdialog.ui \
    axisdialog.ui \
    saveasdialog.ui \
    diffdialog.ui \
    metricsdialog.ui

# ADD THIS LINE!
win32: RC_FILE += app.rc

win32:CONFIG(release, debug|release) {
    QMAKE_EXTRA_TARGETS += releaseAppIconResourceDependency
    releaseAppIconResourceDependency.target = release/app_res.o
    releaseAppIconResourceDependency.depends = $$PWD/images/SeiSeeMp.ico $$PWD/images/segyfile.ico
}
win32:CONFIG(debug, debug|release) {
    QMAKE_EXTRA_TARGETS += debugAppIconResourceDependency
    debugAppIconResourceDependency.target = debug/app_res.o
    debugAppIconResourceDependency.depends = $$PWD/images/SeiSeeMp.ico $$PWD/images/segyfile.ico
}

RESOURCES += \
    myrc.qrc

INCLUDEPATH += $$PWD/../third_party/qt-toast/src
DEPENDPATH += $$PWD/../third_party/qt-toast/src

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../third_party/qt-toast/release/ -lqttoast
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../third_party/qt-toast/debug/ -lqttoast
else:unix: LIBS += -L$$OUT_PWD/../third_party/qt-toast/ -lqttoast

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../third_party/qt-toast/release/libqttoast.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../third_party/qt-toast/debug/libqttoast.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../third_party/qt-toast/release/qttoast.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../third_party/qt-toast/debug/qttoast.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../third_party/qt-toast/libqttoast.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../GxLib/release/ -lGxLib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../GxLib/debug/ -lGxLib
else:unix: LIBS += -L$$OUT_PWD/../GxLib/ -lGxLib

INCLUDEPATH += $$PWD/../GxLib
DEPENDPATH += $$PWD/../GxLib
INCLUDEPATH += $$PWD/../CustomWdgets
DEPENDPATH += $$PWD/../CustomWdgets

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../GxLib/release/libGxLib.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../GxLib/debug/libGxLib.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../GxLib/release/GxLib.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../GxLib/debug/GxLib.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../GxLib/libGxLib.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libUtil2/release/ -llibUtil2
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libUtil2/debug/ -llibUtil2
else:unix: LIBS += -L$$OUT_PWD/../libUtil2/ -llibUtil2

INCLUDEPATH += $$PWD/../libUtil2
DEPENDPATH += $$PWD/../libUtil2

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/release/liblibUtil2.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/debug/liblibUtil2.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/release/libUtil2.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/debug/libUtil2.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/liblibUtil2.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libUtil2/release/ -llibUtil2
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libUtil2/debug/ -llibUtil2
else:unix: LIBS += -L$$OUT_PWD/../libUtil2/ -llibUtil2

INCLUDEPATH += $$PWD/../libUtil2
DEPENDPATH += $$PWD/../libUtil2

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/release/liblibUtil2.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/debug/liblibUtil2.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/release/libUtil2.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/debug/libUtil2.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../libUtil2/liblibUtil2.a

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libExprEval/release/ -llibExprEval
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libExprEval/debug/ -llibExprEval
else:unix: LIBS += -L$$OUT_PWD/../libExprEval/ -llibExprEval

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../libExprEval/release/ -llibExprEval
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../libExprEval/debug/ -llibExprEval
else:unix: LIBS += -L$$OUT_PWD/../libExprEval/ -llibExprEval

INCLUDEPATH += $$PWD/../libExprEval
DEPENDPATH += $$PWD/../libExprEval

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/release/liblibExprEval.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/debug/liblibExprEval.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/release/libExprEval.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/debug/libExprEval.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/liblibExprEval.a

win32:LIBS += -ladvapi32

# Optional regression executable; the default build remains the application.
contains(CONFIG, metrics_regression) {
    TARGET = metrics_regression
    QT += testlib
    CONFIG += console testcase
    SOURCES -= main.cpp
    SOURCES += $$PWD/../tests/upstream_metrics_regression.cpp
}
