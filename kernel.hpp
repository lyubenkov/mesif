#ifndef KERNEL_HPP
#define KERNEL_HPP

#include <QObject>
#include <QThread>
#include <QtWidgets>

#include "global.hpp"

class QTableWidget;

class Kernel : public QThread {
		Q_OBJECT
	public:
		explicit Kernel(int id, QObject *parent = 0);

		void run();											// Выполняется при запуске потока
		void stop();										// Остановка выполнения

		QTableWidget	*cacheTable;						// Таблица кэша
		QTableWidget	*actionTable;						// Таблица заданий
		int				id = -1;							// Идентификатор
		int				timer = 0;							// Таймер для управления работой
		int				actionCount = 0;					// Счётчик заданий

	signals:
		void sendMessage(int id, quint32 num, Mesif::Messages type, QString value = "");

	public slots:
		void timerEvent(QTimerEvent *);						// Обработчик задания по таймеру

		void receiveMessage(int id, quint32 num, Mesif::Messages type, QString value = "");

		void setCacheSize(int size);						// Установка количества ЯП
		void addAction(quint32 adr, bool rw, QString val);	// Дабавление нового пункта в задание
		void clearActionList();								// Очистка списка заданий

	private:
		Action getAction();									// Взять строку задания
		void setCell(Cell cell, quint32 num);				// Установка ЯП кэш в num-ячейку таблицы
		Cell getCell(quint32 num);							// Запрос ЯП по num-ячейки таблицы
		int cacheHit(Cell cell);							// Попадание/промах в кэш
		int getEmptyCell();									// Поиск пустой ЯП в кэш

		int					requestLast = -1;				// Последний запрос
		Cell				cellCurrent;					// Текущая ЯП кэш
		QVector<Message>	message;						// Принятые сообщения
};
#endif // KERNEL_HPP
