#ifndef RAM_HPP
#define RAM_HPP

#include <QObject>
#include <QtWidgets>

#include "global.hpp"

class QString;
class QTableWidget;

class Ram : public QObject {
		Q_OBJECT
	public:
		explicit Ram(QObject *parent = 0);

		void start();										/// Запуск работы ОП
		void stop();										/// Остановка работы ОП

		QTableWidget	*ramTable;
		int				messageCount;						// Количество сообщений в буфере

	signals:
		void sendMessage(int id, quint32 address, Mesif::Messages type, QString value = "");

	public slots:
		void timerEvent(QTimerEvent *);						/// Эмулятор контроллера ОП

		void receiveMessage(int id, quint32 address, Mesif::Messages type, QString value = "");

		void setRamSize(int size);							/// Устfновка количества ячеек в ОП
		void setValue(quint32 address, QString value);		/// Установка значения в ЯП

	private:
		QVector<Message>	message;						// Принятые сообщения
		int					timer = 0;
};

#endif // RAM_HPP
