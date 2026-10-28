#-------------------------------------------------
#
# Project created by QtCreator 2015-06-23T10:08:09
#
#-------------------------------------------------

QT       += widgets

TARGET = GxLib
TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++11

SOURCES += \
    myscrollarea.cpp \
    mystringtable.cpp \
    furlib.cpp \
    myobjptrlist.cpp \
    gfx.cpp \
    gfxobj.cpp \
    gfxobjseis.cpp \
    gfxobjseishdrs.cpp \
    gfxobjseissect.cpp \
    gfxobjtaxis.cpp \
    gfxtimelabel.cpp \
    gfxsctlist.cpp \
    gfxsrc.cpp \
    gfxsrcseis.cpp \
    gfxsrcseismem.cpp \
    gfxsrcseismemsegd.cpp \
    gfxsrcseistest.cpp \
    gfxutil.cpp \
    gfxview.cpp \
    gfxsrcfile.cpp \
    seisfile.cpp \
    sgyfile.cpp \
    hdrdef.cpp \
    gfxobjhsrslab.cpp \
    seistrexpr.cpp \
    mxstringtable.cpp \
    myhugetable.cpp

HEADERS += \
    myscrollarea.h \
    mystringtable.h \
    furlib.h \
    myobjptrlist.h \
    gfx.h \
    gfxstyle.h \
    gfxobj.h \
    gfxobjseis.h \
    gfxobjseishdrs.h \
    gfxobjseissect.h \
    gfxobjtaxis.h \
    gfxtimelabel.h \
    gfxsctlist.h \
    gfxsrc.h \
    gfxsrcseis.h \
    gfxsrcseismem.h \
    gfxsrcseismemsegd.h \
    gfxsrcseistest.h \
    gfxutil.h \
    gfxview.h \
    gfxsrcfile.h \
    seisfile.h \
    sgyfile.h \
    hdrdef.h \
    gfxobjhsrslab.h \
    seistrexpr.h \
    mxstringtable.h \
    myhugetable.h


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

INCLUDEPATH += $$PWD/../libExprEval
DEPENDPATH += $$PWD/../libExprEval

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/release/liblibExprEval.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/debug/liblibExprEval.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/release/libExprEval.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/debug/libExprEval.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../libExprEval/liblibExprEval.a

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

HEADERS += gfxselectionband.h

DISTFILES +=
