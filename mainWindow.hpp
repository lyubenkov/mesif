#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QtWidgets>

#include "kernel.hpp"
#include "ram.hpp"

#include "ui_mainWindow.h"

namespace Ui {
	class MainWindow;
}

class Kernel;
class Ram;

class MainWindow : public QMainWindow {
		Q_OBJECT
	public:
		explicit MainWindow(QWidget *parent = 0);
		~MainWindow();

	signals:
		void sendMessage(int id, quint32 num, Mesif::Messages type, QString value = "");
		void newAction(quint32 adr, bool rw, QString val);

		void newCacheSize(int size);					// Новый размер кэша

	private slots:
		void timerEvent(QTimerEvent *);
		void receiveMessage(int id, quint32 num, Mesif::Messages type, QString value = "");

		void on_startSimulationButton_clicked();		// Кнопки управления
		void on_stepSimulationButton_clicked();
		void on_pauseSimulationButton_clicked();
		void on_stopSimulationButton_clicked();

		void on_applyButton_clicked();					// Кнопки настройки
		void on_openTaskButton_clicked();

		void on_clearOutputButton_clicked();			// Кнопка вывода

		void on_configButton_clicked();					// Кнопки вкладок
		void on_actionButton_clicked();
		void on_outputButton_clicked();

	private:
		Ui::MainWindow *ui;

		Kernel	*kernel[32];							// Массив ядер
		Ram		*ram;									// Основная память

		QVector<Message>	message;					// Сообщения
		QVector<int>		queueTimeout;				// Задержка отправки ЯП

		int					timer = 0;
		int					controlMode = 1;			// Режим нижней панели
		int					mode = 0;					// Режим запуск, пауза...
};

#endif // MAINWINDOW_HPP
