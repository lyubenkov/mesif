#-------------------------------------------------
#	Симулятор работы протокола MESIF
#	Версия:	0.1.0
#	Дата:	29.05.2014
#-------------------------------------------------

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = simul_mesif
TEMPLATE = app
VERSION = 0.1.0

CONFIG += c++11 mobility
MOBILITY =

SOURCES += \
	main.cpp \
	mainWindow.cpp \
	kernel.cpp \
	ram.cpp

HEADERS += \
	global.hpp \
	mainWindow.hpp \
	kernel.hpp \
	ram.hpp

FORMS += mainWindow.ui

RESOURCES += \
	resources.qrc
