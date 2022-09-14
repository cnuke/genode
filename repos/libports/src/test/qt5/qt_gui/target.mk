QMAKE_PROJECT_FILE = $(PRG_DIR)/qt_gui.pro

QMAKE_TARGET_BINARIES = test-qt_gui

QT5_PORT_LIBS = libQt5Core libQt5Gui libQt5Widgets

LIBS = libc libm mesa qt5_component stdcxx $(QT5_PORT_LIBS)

include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)
