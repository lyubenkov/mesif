#include "kernel.hpp"

Kernel::Kernel(int id, QObject *parent) :
	QThread(parent) {

	this->id = id;

	cacheTable = new QTableWidget;
	cacheTable->setColumnCount(4);
	QStringList horizontalLables;
	horizontalLables << tr("*");
	horizontalLables << tr("№");
	horizontalLables << tr("Сост.");
	horizontalLables << tr("Значение");
	cacheTable->setHorizontalHeaderLabels(horizontalLables);
	cacheTable->setColumnWidth(0, 0);
	cacheTable->setColumnWidth(1, 32);
	cacheTable->setColumnWidth(2, 48);
	cacheTable->horizontalHeader()->setStretchLastSection(true);

	actionTable = new QTableWidget;
	actionTable->setColumnCount(3);
	QStringList horizontalLables2;
	horizontalLables2 << tr("№");
	horizontalLables2 << tr("Ч/З");
	horizontalLables2 << tr("Значение");
	actionTable->setHorizontalHeaderLabels(horizontalLables2);
	actionTable->setColumnWidth(0, 32);
	actionTable->setColumnWidth(1, 32);
	actionTable->horizontalHeader()->setStretchLastSection(true);
}

void Kernel::run() {
	timer = startTimer(1000);
}

void Kernel::stop() {
	killTimer(timer);
	timer = 0;
}

void Kernel::timerEvent(QTimerEvent *) {
	/* Обработка жизни ЯП кэш */
	for(int i=0; i<cacheTable->rowCount(); i++) {
		int count = cacheTable->item(i, 0)->text().toInt();
		if(count > 0) {
			cacheTable->item(i, 0)->setText(QString::number(count-1));
		}
	}

	/* Обработчик сообщений */
	while(message.length()) {
		int id = message.first().id;
		quint32 num = message.first().adr;
		Mesif::Messages type = message.first().type;
		QString value = QString::number(message.first().val);
		message.removeFirst();

		switch(type) {
			case Mesif::MessageRequestCell: {
				if(requestLast != Mesif::MessageRequestCell || cellCurrent.adr != num || this->id != id) {
					qDebug() << "Ядро"<<this->id<<": принят запрос ЯП." << id << num;
					Cell cell; cell.adr = num;
					int hit = cacheHit(cell);										// Поиск ЯП в кэш
					if(hit != -1) {
						cell = getCell(hit);
						switch(cell.state) {
							case Mesif::StateModified:
							case Mesif::StateExclusive:
							case Mesif::StateForward:
								emit sendMessage(id, cell.adr, Mesif::MessageAbortRequestRam);
								cell.state = Mesif::StateShared;
								setCell(cell, hit);
								emit sendMessage(id, cell.adr, Mesif::MessageValueCell, QString::number(cell.val));
								break;
							default: break;
						}
					}
				}
				break;
			}
			case Mesif::MessageAbortRequestCell:
				if(requestLast == Mesif::MessageRequestCell && cellCurrent.adr == num) {
					qDebug() << "Ядро"<<this->id<<": отмена запроса ЯП." << id << num;
					requestLast = -1;
				}
				break;

			case Mesif::MessageValueCell: {
				if(requestLast == Mesif::MessageRequestCell && cellCurrent.adr == num && (this->id == id || id == -1)) {
					qDebug() << "Ядро"<<this->id<<": принята ЯП." << id << num << "=" << value;
					requestLast = -1;
					emit sendMessage(-1, num, Mesif::MessageAbortRequestRam);
					emit sendMessage(-1, num, Mesif::MessageAbortRequestCell);

					int emp = getEmptyCell();
					Cell cell = getCell(emp);
					if(cell.state == Mesif::StateModified)
						emit sendMessage(id, cell.adr, Mesif::MessageValueCell, QString::number(cell.val));
					cellCurrent.adr = num;
					cellCurrent.state = Mesif::StateForward;
					cellCurrent.val = value.toUInt();
					setCell(cellCurrent, emp);
				}
				break;
			}
			case Mesif::MessageValueCellRam: {
				if(requestLast == Mesif::MessageRequestCell && cellCurrent.adr == num && (this->id == id || id == -1)) {
					qDebug() << "Ядро"<<this->id<<": принята ЯП от ОП." << id << num << "=" << value;
					requestLast = -1;
					emit sendMessage(-1, num, Mesif::MessageAbortRequestRam);
					emit sendMessage(-1, num, Mesif::MessageAbortRequestCell);

					int emp = getEmptyCell();
					Cell cell = getCell(emp);
					if(cell.state == Mesif::StateModified)
						emit sendMessage(id, cell.adr, Mesif::MessageValueCell, QString::number(cell.val));
					cellCurrent.adr = num;
					cellCurrent.state = Mesif::StateExclusive;
					cellCurrent.val = value.toUInt();
					setCell(cellCurrent, emp);
				}
				break;
			}
			case Mesif::MessageSetInvalid: {
				qDebug() << "Ядро"<<this->id<<": принята установка в I." << id << num;
				if(this->id != id) {
					if(requestLast == Mesif::MessageSetInvalid) {
						requestLast = -1;
					} else {
						Cell cellQuery; cellQuery.adr = num;
						int hit = cacheHit(cellQuery);								// Поиск ЯП в кэш
						if(hit != -1) {
							Cell cell = getCell(hit);
							cell.state = Mesif::StateInvalid;
							setCell(cell, hit);
						}
					}
				}
				break;
			}
			default: break;
		}
	} // while(...)

	if(actionCount >= actionTable->rowCount()) return;							// Если закончились задания, выход

	/* Выполнение задания */
	Action act = getAction();
	Cell cellQuery; cellQuery.adr = act.adr;
	int hit = cacheHit(cellQuery);												// Поиск ЯП в кэш

	if(hit != -1) {																// Попадание в кэш
		qDebug() << "Ядро"<<this->id<<": попадание в кэш" << act.adr;
		cellCurrent = getCell(hit);
	} else {																	// Промах в кэш
		if(requestLast == -1) {
			qDebug() << "Ядро"<<this->id<<": промах в кэш" << act.adr;
			emit sendMessage(this->id, act.adr, Mesif::MessageRequestCell);
			cellCurrent.adr = act.adr;
			requestLast = Mesif::MessageRequestCell;
		}
		return;																	// Ожидание ЯП
	}

	if(!act.rw) {																// Чтение
		qDebug() << "Ядро"<<this->id<<": прочитана ячейка" << hit;
	} else {																	// Запись
		qDebug() << "Ядро"<<this->id<<": записана ячейка" << hit;
		switch(cellCurrent.state) {
			case Mesif::StateExclusive:
				emit sendMessage(id, cellCurrent.adr, Mesif::MessageSetInvalid);
				cellCurrent.state = Mesif::StateModified;

			case Mesif::StateModified:
				cellCurrent.val = act.val;
				setCell(cellCurrent, hit);
				break;

			case Mesif::StateShared:
			case Mesif::StateForward:
				emit sendMessage(id, cellCurrent.adr, Mesif::MessageSetInvalid);
				requestLast = Mesif::MessageSetInvalid;
				cellCurrent.state = Mesif::StateModified;
				cellCurrent.val = act.val;
				setCell(cellCurrent, hit);
				break;

			default: break;
		}
	}

	/* Перекраска выполненного задания */
	actionTable->item(actionCount, 0)->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
	actionTable->item(actionCount, 1)->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
	actionTable->item(actionCount, 2)->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
	actionCount++;
}

void Kernel::receiveMessage(int id, quint32 num, Mesif::Messages type, QString value) {
	Message mesNew;
	mesNew.id = id;
	mesNew.adr = num;
	mesNew.type = type;
	mesNew.val = value.toInt();
	message.append(mesNew);
}

void Kernel::setCacheSize(int size) {
	int rowCountLast = cacheTable->rowCount();
	cacheTable->setRowCount(size);

	for(int i = rowCountLast; i < cacheTable->rowCount(); i++) {			// Добавление новых ЯП в кэш
		QTableWidgetItem *countItem = new QTableWidgetItem;
		countItem->setText(QString::number(0));
		countItem->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
		cacheTable->setItem(i, 0, countItem);

		QTableWidgetItem *numItem = new QTableWidgetItem;
		numItem->setText(QString::number(0));
		numItem->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
		cacheTable->setItem(i, 1, numItem);

		QTableWidgetItem *stateItem = new QTableWidgetItem;
		stateItem->setText("I");
		stateItem->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
		cacheTable->setItem(i, 2, stateItem);

		QTableWidgetItem *valueItem = new QTableWidgetItem;
		valueItem->setText("0");
		valueItem->setBackground(QBrush(QColor(0xff, 0xcc, 0xcc)));
		cacheTable->setItem(i, 3, valueItem);
	}
}

void Kernel::addAction(quint32 adr, bool rw, QString val) {
	actionTable->insertRow(actionTable->rowCount());

	QTableWidgetItem *adrItem = new QTableWidgetItem;
	adrItem->setText(QString::number(adr));
	adrItem->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
	actionTable->setItem(actionTable->rowCount()-1, 0, adrItem);

	QTableWidgetItem *rwItem = new QTableWidgetItem;
	rwItem->setText(!rw ? tr("Ч") : tr("З"));
	rwItem->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
	actionTable->setItem(actionTable->rowCount()-1, 1, rwItem);

	QTableWidgetItem *valItem = new QTableWidgetItem;
	valItem->setText(val);
	valItem->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
	actionTable->setItem(actionTable->rowCount()-1, 2, valItem);
}

void Kernel::clearActionList() {
	actionTable->setRowCount(0);
}

Action Kernel::getAction() {
	Action act;
	act.adr = actionTable->item(actionCount, 0)->text().toUInt();
	act.rw = actionTable->item(actionCount, 1)->text() == "Ч" ? false : true;
	act.val = actionTable->item(actionCount, 2)->text().toInt();
	return act;
}

void Kernel::setCell(Cell cell, quint32 num) {
	QBrush color;
	cacheTable->item(num, 0)->setText(QString::number(32));
	cacheTable->item(num, 1)->setText(QString::number(cell.adr));
	switch(cell.state) {
		case Mesif::StateModified:	cacheTable->item(num, 2)->setText("M");	color = QBrush(QColor(0xff, 0xff, 0xcc));	break;
		case Mesif::StateExclusive:	cacheTable->item(num, 2)->setText("E");	color = QBrush(QColor(0xcc, 0xcc, 0xff));	break;
		case Mesif::StateShared:	cacheTable->item(num, 2)->setText("S");	color = QBrush(QColor(0xcc, 0xff, 0xcc));	break;
		case Mesif::StateInvalid:	cacheTable->item(num, 2)->setText("I");	color = QBrush(QColor(0xff, 0xcc, 0xcc));	break;
		case Mesif::StateForward:	cacheTable->item(num, 2)->setText("F");	color = QBrush(QColor(0xcc, 0xff, 0xff));	break;
	}
	cacheTable->item(num, 3)->setText(QString::number(cell.val));

	for(int i=0; i<cacheTable->columnCount(); i++)
		cacheTable->item(num, i)->setBackground(color);
}

Cell Kernel::getCell(quint32 num) {
	Cell cell;
	cell.adr = cacheTable->item(num, 1)->text().toUInt();
	if(cacheTable->item(num, 2)->text() == "M")			cell.state = Mesif::StateModified;
	else if(cacheTable->item(num, 2)->text() == "E")	cell.state = Mesif::StateExclusive;
	else if(cacheTable->item(num, 2)->text() == "S")	cell.state = Mesif::StateShared;
	else if(cacheTable->item(num, 2)->text() == "I")	cell.state = Mesif::StateInvalid;
	else if(cacheTable->item(num, 2)->text() == "F")	cell.state = Mesif::StateForward;
	cell.val = cacheTable->item(num, 3)->text().toUInt();
	return cell;
}

int Kernel::cacheHit(Cell cell) {
	for(int i=0; i<cacheTable->rowCount(); i++) {
		Cell cellCur = getCell(i);
		if(cellCur.adr == cell.adr && cellCur.state != Mesif::StateInvalid)
			return i;
	}
	return -1;
}

int Kernel::getEmptyCell() {
	for(int i=0; i<cacheTable->rowCount(); i++)
		if(cacheTable->item(i, 2)->text() == "I")
			return i;
	int emp = 0;
	for(int i=0; i<cacheTable->rowCount(); i++) {
		if(cacheTable->item(i, 0)->text().toUInt() == 0)
			return i;
		if(cacheTable->item(emp, 0)->text().toUInt() > cacheTable->item(i, 0)->text().toUInt())
			emp = i;
	}
	return emp;
}
