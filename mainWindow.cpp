#include "mainWindow.hpp"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), ui(new Ui::MainWindow) {

	ui->setupUi(this);
	ui->actionBox->hide();
	ui->outputBox->hide();

	ram = new Ram;
	ui->centralLayout->addWidget(ram->ramTable, 1, 1);
	connect(this, &MainWindow::sendMessage, ram, &Ram::receiveMessage);
	connect(ram, &Ram::sendMessage, this, &MainWindow::receiveMessage);

	on_applyButton_clicked();
}

MainWindow::~MainWindow() {
	ui->kernelCount->setValue(1);
	on_applyButton_clicked();
	kernel[0]->exit();
	kernel[0]->wait();
	delete kernel[0];
	delete ui;
}

void MainWindow::timerEvent(QTimerEvent *) {
	bool stop = true;
	for(int i=0; i<ui->kernelCount->value(); i++)
		if(kernel[i]->actionCount != kernel[i]->actionTable->rowCount())
			stop = false;
	if(!ram->messageCount && stop)
		on_stopSimulationButton_clicked();
	qDebug() << "++" << ram->messageCount;

	if(message.empty() || queueTimeout.empty())
		return;

	/* Обработка таймаута очереди */
	if(queueTimeout.first() > 0) {
		if(message.length() < 2) {
			queueTimeout.first()--;
			return;
		} else {
			message.insert(2, message.first());
			queueTimeout.insert(2, queueTimeout.first()-1);
			message.removeFirst();
			queueTimeout.removeFirst();
		}
	}

	/* Пересылка сообщения */
	qDebug() << "Хаб: передача:" << message.first().id << message.first().adr << message.first().type << message.first().val;
	emit sendMessage(message.first().id, message.first().adr, message.first().type, QString::number(message.first().val));
	ui->textBrowser->append(tr("Хаб: передача (%1): %2 [%3] \"%4\"").arg(message.first().id).arg(message.first().adr)
							.arg(message.first().type).arg(message.first().val));

	message.removeFirst();
	queueTimeout.removeFirst();
}

void MainWindow::receiveMessage(int id, quint32 address, Mesif::Messages type, QString value) {
	qDebug() << "Хаб: приём:" << id << address << type << value;
	ui->textBrowser->append(tr("Хаб: приём (%1): %2 [%3] \"%4\"").arg(id).arg(address).arg(type).arg(value));

	Message mesNew;
	mesNew.id = id;
	mesNew.adr = address;
	mesNew.type = type;
	mesNew.val = value.toInt();
	message.append(mesNew);
	queueTimeout.append(0);
}

void MainWindow::on_startSimulationButton_clicked() {
	if(mode != 1) {
		mode = 1;
		on_applyButton_clicked();
		for(int i = 0; i < ui->kernelCount->value(); i++) {
#ifndef Q_OS_WIN
			kernel[i]->start();
#else
			kernel[i]->timer = kernel[i]->startTimer(1000);
#endif
		}
		timer = startTimer(100);
		ram->start();
	}
}

void MainWindow::on_stepSimulationButton_clicked() {
	for(int i = 0; i < ui->kernelCount->value(); i++) {
		QTimerEvent *event = new QTimerEvent(1);
		kernel[i]->timerEvent(event);
	}
	QTimerEvent *event = new QTimerEvent(1);
	timerEvent(event);
	ram->timerEvent(event);
}

void MainWindow::on_pauseSimulationButton_clicked() {
	if(mode != 2) {
		mode = 2;
		for(int i = 0; i < ui->kernelCount->value(); i++) {
			kernel[i]->stop();
#ifdef Q_OS_WIN
			kernel[i]->killTimer(kernel[i]->timer);
			kernel[i]->timer = 0;
#endif
		}
		killTimer(timer);
		timer = 0;
		ram->stop();
	}

}

void MainWindow::on_stopSimulationButton_clicked() {
	mode = 0;
	for(int i = 0; i < ui->kernelCount->value(); i++) {
		kernel[i]->stop();
		kernel[i]->actionCount = 0;
		for(int j = 0; j < kernel[i]->actionTable->rowCount(); j++) {
			kernel[i]->actionTable->item(j, 0)->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
			kernel[i]->actionTable->item(j, 1)->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
			kernel[i]->actionTable->item(j, 2)->setBackground(QBrush(QColor(0xcc, 0xff, 0xcc)));
		}
#ifdef Q_OS_WIN
		kernel[i]->killTimer(kernel[i]->timer);
		kernel[i]->timer = 0;
#endif
	}
	killTimer(timer);
	timer = 0;
	ram->stop();
}

void MainWindow::on_applyButton_clicked() {
	static int kernelCountLast;

	ram->setRamSize(ui->ramCount->value());

	for(int i = ui->kernelCount->value(); i < kernelCountLast; i++) {				// Удаление лишних ядер
		kernel[i]->cacheTable->hide();
		ui->kernelAreaLayout->removeWidget(kernel[i]->cacheTable);
		kernel[i]->actionTable->hide();
		ui->actionLayout->removeWidget(kernel[i]->actionTable);
		kernel[i]->exit();
		kernel[i]->wait();
		delete kernel[i];
	}
	for(int i = kernelCountLast; i < ui->kernelCount->value(); i++) {				// Добавление новых ядер
		kernel[i] = new Kernel(i);
		connect(this, &MainWindow::newCacheSize, kernel[i], &Kernel::setCacheSize);
		connect(this, &MainWindow::sendMessage, kernel[i], &Kernel::receiveMessage);
		connect(kernel[i], &Kernel::sendMessage, this, &MainWindow::receiveMessage);
		ui->kernelAreaLayout->addWidget(kernel[i]->cacheTable, i/2, i%2);
		ui->actionLayout->addWidget(kernel[i]->actionTable, i/2, i%2);
	}

	emit newCacheSize(ui->cacheCount->value());										// Установка количества ячеек кэша
	kernelCountLast = ui->kernelCount->value();
}

void MainWindow::on_openTaskButton_clicked() {
	QFile taskList(QFileDialog::getOpenFileName(0, tr("Симулятор MESIF - Открыть задание"), QDir::currentPath()+"/..",
													tr("Список заданий (*.xml);;"
														"Все файлы (*.*)"), 0));

	if(taskList.open(QIODevice::ReadOnly)) {
		for(int i=0; i < ui->kernelCount->value(); i++)
			kernel[i]->clearActionList();

		int kernelIndex = -1;
		bool kern = false, ram = false,											// Для кого задание
			action = false,														// Начало блока действия
			adr = false, rw = false, val = false;								// Параметры действия
		quint32 address = 0; bool readWrite = false; QString value = "";		// Параметры задания

		while(!taskList.atEnd()) {
			/* Чтение и форматирование строки */
			QByteArray buffer = taskList.readLine().replace('	', ' ');
			buffer.remove(buffer.indexOf('#'), buffer.length());				// Удаление комментаря
			while(buffer[0] == ' ') buffer.remove(0, 1);						// Удаление пробелов в начале строки
			buffer.remove(buffer.length()-1, 1);								// Удаление переноса в конце строки
			if(buffer.mid(buffer.length()-1).contains(0x0d)) buffer.remove(buffer.length()-1, 1);
			for(int j=0; j<buffer.length(); j++) {								// Удаление подряд идущих пробелов
				if(j<buffer.length() && buffer[j]==' ' && buffer[j-1]==' ') {
					buffer.remove(j-1, 1);
					j--;
				}
			}

			/* Анализ строки */
			if(buffer.contains("<kernel>")) {
				kern = true;
				kernelIndex++;
				if(kernelIndex > -1 && kernelIndex < ui->kernelCount->value())
					connect(this, &MainWindow::newAction, kernel[kernelIndex], &Kernel::addAction);
			} else if(buffer.contains("</kernel>")) {
				kern = false;
				disconnect(this, &MainWindow::newAction, kernel[kernelIndex], &Kernel::addAction);

			} else if(buffer.contains("<ram>")) {		ram = true;
			} else if(buffer.contains("</ram>")) {		ram = false;

			} else if(buffer.contains("<action>")) {
				if(kern || ram)							action = true;
			} else if(buffer.contains("</action>")) {	action = false;
				if(kern) {
					qDebug() << "Передать действие ядру:" << kernelIndex << address << readWrite << value;
					emit newAction(address, readWrite, value);
				} else if(ram && this->ram->ramTable->rowCount() > (int)address) {
					this->ram->setValue(address, value);
				}

			} else if(buffer.contains("<adr>")) {
				buffer.remove(0, 5);
				if((kern || ram) && action)			adr = true;
			} else if(buffer.contains("</adr>")) {	adr = false;
			} else if(buffer.contains("<rw>")) {
				buffer.remove(0, 4);
				if((kern || ram) && action)			rw = true;
			} else if(buffer.contains("</rw>")) {	rw = false;
			} else if(buffer.contains("<val>")) {
				buffer.remove(0, 5);
				if((kern || ram) && action)			val = true;
			} else if(buffer.contains("</val>")) {	val = false;
			}

			/* Чтение параметров заданий */
			if(action) {
				if(adr) {
					if(buffer.contains("</adr>")) adr = false;
					address = buffer.remove(buffer.indexOf('/')-2, buffer.length()).toUInt();
				} else if(rw) {
					if(buffer.contains("</rw>")) rw = false;
					readWrite = buffer.remove(buffer.indexOf('/')-2, buffer.length()).toUInt();
				} else if(val) {
					if(buffer.contains("</val>")) val = false;
					value = QString::number(buffer.remove(buffer.indexOf('/')-2, buffer.length()).toUInt());
				}
			}
		} // while(...)
	} // if(...)
}

void MainWindow::on_clearOutputButton_clicked() {
	ui->textBrowser->clear();
}

void MainWindow::on_configButton_clicked() {
    ui->actionBox->hide();
    ui->outputBox->hide();
    if(controlMode != 1) {
		ui->configBox->show();
        controlMode = 1;
    } else {
		ui->configBox->hide();
        controlMode = 0;
    }
}

void MainWindow::on_actionButton_clicked() {
	ui->configBox->hide();
    ui->outputBox->hide();
    if(controlMode != 2) {
        ui->actionBox->show();
        controlMode = 2;
    } else {
        ui->actionBox->hide();
        controlMode = 0;
    }
}

void MainWindow::on_outputButton_clicked() {
    ui->actionBox->hide();
	ui->configBox->hide();
    if(controlMode != 3) {
        ui->outputBox->show();
        controlMode = 3;
    } else {
        ui->outputBox->hide();
        controlMode = 0;
    }

}
