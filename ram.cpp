#include "ram.hpp"

Ram::Ram(QObject *parent) :
	QObject(parent) {

	ramTable = new QTableWidget;
	ramTable->setColumnCount(2);
	QStringList horizontalLables;
	horizontalLables << tr("№");
	horizontalLables << tr("Значение");
	ramTable->setHorizontalHeaderLabels(horizontalLables);
	ramTable->setColumnWidth(0, 32);
	ramTable->horizontalHeader()->setStretchLastSection(true);
}

void Ram::start() {
	timer = startTimer(4096);
}

void Ram::stop() {
	killTimer(timer);
	timer = 0;
}

void Ram::timerEvent(QTimerEvent *) {
	messageCount = message.length();
	if(message.empty()) return;												// Если нет сообщений

	/* Действие по сообщению */
	switch(message.first().type) {
		case Mesif::MessageRequestCell:
			qDebug() << "ОП: чтение." << message.first().id << message.first().adr;
			emit sendMessage(message.first().id, message.first().adr, Mesif::MessageValueCellRam, ramTable->item(message.first().adr, 1)->text());
			break;
		case Mesif::MessageValueCell:
			qDebug() << "ОП: запись." << message.first().id << message.first().adr << "=" << message.first().val;
			setValue(message.first().adr, QString::number(message.first().val));
			break;
		default: break;
	}
	message.removeFirst();
}

void Ram::receiveMessage(int id, quint32 address, Mesif::Messages type, QString value) {
	if(type == Mesif::MessageRequestCell || type == Mesif::MessageValueCell || type == Mesif::MessageAbortRequestRam) {
		Message mesNew;
		mesNew.id = id;
		mesNew.adr = address;
		mesNew.type = type;
		mesNew.val = value.toInt();
		message.append(mesNew);

		/* Обработка сообщений */
		if(message.last().type == Mesif::MessageAbortRequestRam) {
			for(int i=0; i<message.length(); i++)
				if(message[i].type == Mesif::MessageRequestCell && message.last().adr == message[i].adr)
					message.remove(i);
			message.removeLast();
		}
	}
}

void Ram::setRamSize(int size) {
	int ramSizeLast = ramTable->rowCount();

	ramTable->setRowCount(size);
	for(int i = ramSizeLast; i < size; i++) {
		QTableWidgetItem *adrItem = new QTableWidgetItem;
		adrItem->setText(QString::number(i));
		ramTable->setItem(i, 0, adrItem);

		QTableWidgetItem *valueItem = new QTableWidgetItem;
		valueItem->setText("0");
		ramTable->setItem(i, 1, valueItem);
	}
}

void Ram::setValue(quint32 address, QString value) {
	ramTable->item(address, 0)->setText(QString::number(address));
	ramTable->item(address, 1)->setText(value);
}
