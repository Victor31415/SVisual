//
// SVisual Project
// Copyright (C) 2018 by Contributors <https://github.com/Tyill/SVisual>
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "stdafx.h"
#include <QSettings>
#include <QSystemTrayIcon>
#include <QPrinter>
#include <QPrintDialog>
#include "forms/mainwin.h"
#include "forms/eventOrderWin.h"
#include "forms/settingsPanel.h"
#include "forms/graphSettingPanel.h"
#include "sql.h"
#include "comReader.h"
#include "SVAuxFunc/mt_log.h"
#include "SVAuxFunc/serverTCP.h"
#include "SVGraphPanel/SVGraphPanel.h"
#include "SVExportPanel/SVExportPanel.h"
#include "SVTriggerPanel/SVTriggerPanel.h"
#include "SVScriptPanel/SVScriptPanel.h"
#include "SVServer/SVServer.h"
#include "serverAPI.h"

const QString VERSION = "1.0.9";
// -add setting graph view 

// const QString VERSION = "1.0.8";
// -add script panel

// const QString VERSION = "1.0.7";
// -font change

// const QString VERSION = "1.0.6";
// -save win state
// -small fix's

MainWin* mainWin = nullptr;

using namespace SV_Cng;

void statusMess(QString mess){

    QMetaObject::invokeMethod(mainWin->ui.txtStatusMess, "append", Qt::AutoConnection,
        Q_ARG(QString, QString::fromStdString(SV_Aux::CurrDateTime()) + " " + mess));

    mainWin->lg.WriteLine(qPrintable(mess));
}

void MainWin::load(){
		
	ui.treeSignals->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.treeSignals->setIconSize(QSize(40, 20));	

	orderWin_ = new eventOrderWin(this); orderWin_->setWindowFlags(Qt::Window);
	settPanel_ = new settingsPanel(this); settPanel_->setWindowFlags(Qt::Window);
    graphSettPanel_ = new graphSettingPanel(this, cng.graphSett); graphSettPanel_->setWindowFlags(Qt::Window);
    triggerPanel_ = SV_Trigger::createTriggerPanel(this, SV_Trigger::config(cng.cycleRecMs, cng.packetSz));
    triggerPanel_->setWindowFlags(Qt::Window);
    exportPanel_ = SV_Exp::createExpPanel(this, SV_Exp::config(cng.cycleRecMs, cng.packetSz));
    exportPanel_->setWindowFlags(Qt::Window);
    scriptPanel_ = SV_Script::createScriptPanel(this, SV_Script::config(cng.cycleRecMs, cng.packetSz), SV_Script::modeGr::player);
    scriptPanel_->setWindowFlags(Qt::Window);
       
    SV_Graph::setLoadSignalData(graphPanels_[this], loadSignalDataSrv);
    SV_Graph::setGetCopySignalRef(graphPanels_[this], getCopySignalRefSrv);
    SV_Graph::setGetSignalData(graphPanels_[this], getSignalDataSrv);
    SV_Graph::setGraphSetting(graphPanels_[this], cng.graphSett);

    SV_Trigger::setGetCopySignalRef(triggerPanel_, getCopySignalRefSrv);
    SV_Trigger::setGetSignalData(triggerPanel_, getSignalDataSrv);
    SV_Trigger::setGetCopyModuleRef(triggerPanel_, getCopyModuleRefSrv);
    SV_Trigger::setGetModuleData(triggerPanel_, getModuleDataSrv);
    SV_Trigger::setOnTriggerCBack(triggerPanel_, [](const QString& name){
        mainWin->onTrigger(name);
    });

    SV_Script::setLoadSignalData(scriptPanel_, loadSignalDataSrv);
    SV_Script::setGetCopySignalRef(scriptPanel_, getCopySignalRefSrv);
    SV_Script::setGetSignalData(scriptPanel_, getSignalDataSrv);
    SV_Script::setGetModuleData(scriptPanel_, getModuleDataSrv);
    SV_Script::setAddSignal(scriptPanel_, addSignalSrv);
    SV_Script::setAddModule(scriptPanel_, addModuleSrv);    
    SV_Script::setAddSignalsCBack(scriptPanel_, [](){
        QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
    });
    SV_Script::setUpdateSignalsCBack(scriptPanel_, [](){
        QMetaObject::invokeMethod(mainWin, "updateSignals", Qt::AutoConnection);
    });
    SV_Script::setModuleConnectCBack(scriptPanel_, [](const std::string& module){
        QMetaObject::invokeMethod(mainWin, "moduleConnect", Qt::AutoConnection, Q_ARG(QString, QString::fromStdString(module)));
    });

    SV_Exp::setLoadSignalData(exportPanel_, loadSignalDataSrv);
    SV_Exp::setGetCopySignalRef(exportPanel_, getCopySignalRefSrv);
    SV_Exp::setGetCopyModuleRef(exportPanel_, getCopyModuleRefSrv);
    SV_Exp::setGetSignalData(exportPanel_, getSignalDataSrv);

	SV_Srv::setStatusCBack([](const std::string& mess){
	    statusMess(mess.c_str());
	});
	SV_Srv::setOnUpdateSignalsCBack([](){
        QMetaObject::invokeMethod(mainWin, "updateSignals", Qt::AutoConnection);
	});
	SV_Srv::setOnAddSignalsCBack([](){
        QMetaObject::invokeMethod(mainWin, "updateTblSignal", Qt::AutoConnection);
	});
	SV_Srv::setOnModuleConnectCBack([](const std::string& module){
        QMetaObject::invokeMethod(mainWin, "moduleConnect", Qt::AutoConnection, Q_ARG(QString, QString::fromStdString(module)));
	});
	SV_Srv::setOnModuleDisconnectCBack([](const std::string& module){
        QMetaObject::invokeMethod(mainWin, "moduleDisconnect", Qt::AutoConnection, Q_ARG(QString, QString::fromStdString(module)));
	});
  
	bool err = false;
	db = new sql(qUtf8Printable(cng.dbPath), err);

	if (!err) statusMess(tr("Подключение базы данных успешно"));
	else{
		statusMess(tr("Подключение базы данных ошибка: ") + cng.dbPath);
		db = nullptr;
	}

	/////////////////////
	initTrayIcon();

}

void MainWin::Connect(){
		
	connect(ui.treeSignals, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(selSignalClick(QTreeWidgetItem*, int)));
	connect(ui.treeSignals, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(selSignalDClick(QTreeWidgetItem*, int)));
	connect(ui.treeSignals, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(selSignalChange(QTreeWidgetItem*, int)));

	connect(ui.actionExit, &QAction::triggered, [this]() { 
		this->close();
	});
	connect(ui.actionPrint, &QAction::triggered, [this]() {
		
		QPrinter printer(QPrinter::HighResolution);
		printer.setPageMargins(12, 16, 12, 20, QPrinter::Millimeter);
		printer.setFullPage(false);
		
		QPrintDialog printDialog(&printer, this);
		if (printDialog.exec() == QDialog::Accepted) {
			
			QPainter painter(&printer);

            double xscale = printer.pageRect().width() / double(graphPanels_[this]->width());
            double yscale = printer.pageRect().height() / double(graphPanels_[this]->height());
			double scale = qMin(xscale, yscale);
			painter.translate(printer.paperRect().x(), printer.paperRect().y());
			painter.scale(scale, scale);
		
            graphPanels_[this]->render(&painter);
		}
	});
	connect(ui.actionTrgPanel, &QAction::triggered, [this]() {
        if (triggerPanel_) triggerPanel_->showNormal();
	});
	connect(ui.actionEventOrder, &QAction::triggered, [this]() {
        if (orderWin_) orderWin_->showNormal();
	});
    connect(ui.actionExport, &QAction::triggered, [this]() {
        if (exportPanel_) exportPanel_->showNormal();
    });
    connect(ui.actionNewWin, &QAction::triggered, [this]() {
        
        addNewWindow(QRect());
    });
    connect(ui.actionScript, &QAction::triggered, [this]() {
        if (scriptPanel_) scriptPanel_->showNormal();
    });
	connect(ui.actionSettings, &QAction::triggered, [this]() {
        if (settPanel_) settPanel_->showNormal();
	});
    connect(ui.actionGraphSett, &QAction::triggered, [this]() {
        if (graphSettPanel_) graphSettPanel_->showNormal();
    });
    connect(ui.actionUpFont, &QAction::triggered, [this]() {
       
        QFont ft = QApplication::font();
        
        ft.setPointSize(ft.pointSize() + 1);
        
        QApplication::setFont(ft);
    });
    connect(ui.actionDnFont, &QAction::triggered, [this]() {

        QFont ft = QApplication::font();

        ft.setPointSize(ft.pointSize() - 1);
        
        QApplication::setFont(ft);
    });
    connect(ui.actionSaveWinState, &QAction::triggered, [this]() {
       
        QString fname = QFileDialog::getSaveFileName(this,
            tr("Сохранение состояния окон"), cng.selOpenDir,
            "ini files (*.ini)");

        if (fname.isEmpty()) return;
        cng.selOpenDir = fname;

        QFile file(fname);

        QTextStream txtStream(&file);
            
        auto wins = graphPanels_.keys();

        int cnt = 0;
        for (auto w : wins){

            file.open(QIODevice::WriteOnly);
            txtStream << "[graphWin" << cnt << "]" << endl;    

            if (w == this)
                txtStream << "locate = 0" << endl;
            else{        
                auto geom = ((QDialog*)w)->geometry();
                txtStream << "locate = " << geom.x() << " " << geom.y() << " " << geom.width() << " " << geom.height() << endl;
            }

            auto tmIntl = SV_Graph::getTimeInterval(graphPanels_[w]);
            txtStream << "tmDiap = " << (tmIntl.second - tmIntl.first) << endl;

            QVector<QVector<QString>> signs = SV_Graph::getLocateSignals(graphPanels_[w]);
            for (int i = 0; i < signs.size(); ++i){

                txtStream << "section" << i << " = ";
                for (int j = signs[i].size() - 1; j >= 0; --j)
                    txtStream << signs[i][j] << " ";
               
                txtStream << endl;
            }            

            txtStream << endl;
            ++cnt;
        }

        file.close();

        statusMess(tr("Состояние успешно сохранено"));
    });
    connect(ui.actionLoadWinState, &QAction::triggered, [this]() {

        QString fname = QFileDialog::getOpenFileName(this,
            tr("Загрузка состояния окон"), cng.selOpenDir,
            "ini files (*.ini)");

        if (fname.isEmpty()) return;
        cng.selOpenDir = fname;

        QSettings settings(fname, QSettings::IniFormat);

        auto grps = settings.childGroups();
        for (auto& g : grps){
            settings.beginGroup(g);
                      
             QString locate = settings.value("locate").toString();
             QObject* win = this;
             if (locate != "0"){
    
                 auto lt = locate.split(' ');

                 win = addNewWindow(QRect(lt[0].toInt(), lt[1].toInt(), lt[2].toInt(), lt[3].toInt()));                 
             }
                                     
             int sect = 0;
             while (true){

                 QString str = settings.value("section" + QString::number(sect), "").toString();
                 if (str.isEmpty()) break;

                 QStringList signs = str.split(' ');
                 for (auto& s : signs)
                     SV_Graph::addSignal(graphPanels_[win], s, sect);

                 ++sect;
             }
             QString tmDiap = settings.value("tmDiap", "10000").toString();

             auto ctmIntl = SV_Graph::getTimeInterval(graphPanels_[win]);
             ctmIntl.second = ctmIntl.first + tmDiap.toLongLong();

             SV_Graph::setTimeInterval(graphPanels_[win], ctmIntl.first, ctmIntl.second);

            settings.endGroup();
        }
        
        statusMess(tr("Состояние успешно загружено"));
    });
    connect(ui.actionProgram, &QAction::triggered, [this]() {

        QString mess = "<h2>SVMonitor </h2>"
            "<p>Программное обеспечение предназначенное"
            "<p>для анализа сигналов с устройст."
			"<p>2017";

		QMessageBox::about(this, tr("About SVisual"), mess);
	});		
	connect(ui.btnSlowPlay, &QPushButton::clicked, [this]() {
		slowMode();
	});
}

bool MainWin::writeSettings(QString pathIni){

    QFile file(pathIni);

    QTextStream txtStream(&file);

    // запись новых данных
    file.open(QIODevice::WriteOnly);
    txtStream << "[Param]" << endl;
    txtStream << endl;
    txtStream << "tcp_addr = " << cng.tcp_addr << endl;
    txtStream << "tcp_port = " << cng.tcp_port << endl;
    txtStream << endl;
    txtStream << "com_ena = " << (cng.com_ena ? 1 : 0) << endl;
    txtStream << "com_name = " << cng.com_name << endl;
    txtStream << "com_speed = " << cng.com_speed << endl;
    txtStream << endl;
    txtStream << "dbPath = " << cng.dbPath << endl;
    txtStream << endl;
    txtStream << "cycleRecMs = " << cng.cycleRecMs << endl;
    txtStream << "packetSz = " << cng.packetSz << endl;
    txtStream << endl;
    txtStream << "; save on disk" << endl;
    txtStream << "outArchiveEna = " << (cng.outArchiveEna ? "1" : "0") << endl;
    txtStream << "outArchivePath = " << cng.outArchivePath << endl;
    txtStream << "outArchiveName = " << cng.outArchiveName << endl;
    txtStream << endl;
    txtStream << "selOpenDir = " << cng.selOpenDir << endl;
    txtStream << "fontSz = " << this->font().pointSize() << endl;
    txtStream << "transparent = " << cng.graphSett.transparent << endl;
    txtStream << "lineWidth = " << cng.graphSett.lineWidth << endl;

    file.close();

    return true;
}

bool MainWin::init(QString initPath){

    QString logPath = cng.dirPath + "/log/";
    lg.SetPath(qPrintable(logPath));
    lg.SetName("svm");

    QSettings settings(initPath, QSettings::IniFormat);
    settings.beginGroup("Param");

    cng.cycleRecMs =  settings.value("cycleRecMs", 100).toInt();
    cng.cycleRecMs = qMax(cng.cycleRecMs, 10);
    cng.packetSz = settings.value("packetSz", 10).toInt();
    cng.packetSz = qMax(cng.packetSz, 1);

  
    cng.selOpenDir = settings.value("selOpenDir", "").toString();

    QFont ft = QApplication::font();
    int fsz = settings.value("fontSz", ft.pointSize()).toInt();
    ft.setPointSize(fsz);
    QApplication::setFont(ft);
    
    // связь по usb
    cng.com_ena = settings.value("com_ena", 0).toInt() == 1;
    cng.com_name = settings.value("com_name", "COM4").toString();
    cng.com_speed = settings.value("com_speed", 9600).toInt();

    cng.dbPath = settings.value("dbPath", "").toString();
    if (cng.dbPath.isEmpty())cng.dbPath = cng.dirPath + "/svm.db";

    // связь по TCP
    cng.tcp_addr = settings.value("tcp_addr", "127.0.0.1").toString();
    cng.tcp_port = settings.value("tcp_port", "2144").toInt();

    // копир на диск
    cng.outArchiveEna = settings.value("outArchiveEna", "1").toInt() == 1;
    cng.outArchivePath = settings.value("outArchivePath", "").toString();
    if (cng.outArchivePath.isEmpty()) cng.outArchivePath = cng.dirPath + "/";
    cng.outArchiveName = settings.value("outFileName", "svrec").toString();;
    cng.outArchiveHourCnt = settings.value("outFileHourCnt", 2).toInt();
    cng.outArchiveHourCnt = qBound(1, cng.outArchiveHourCnt, 12);

    cng.graphSett.lineWidth = settings.value("lineWidth", "2").toInt();
    cng.graphSett.transparent = settings.value("transparent", "100").toInt();

    settings.endGroup();

    if (!QFile(initPath).exists())
        writeSettings(initPath);

	srvCng.cycleRecMs = cng.cycleRecMs;
	srvCng.packetSz = cng.packetSz;
	srvCng.outArchiveEna = cng.outArchiveEna;
	srvCng.outArchiveHourCnt = cng.outArchiveHourCnt;
	srvCng.outArchiveName = cng.outArchiveName.toStdString();
	srvCng.outArchivePath = cng.outArchivePath.toStdString();

	return true;
}

MainWin::MainWin(QWidget *parent)
	: QMainWindow(parent){

	ui.setupUi(this);

	mainWin = this;

	this->setWindowTitle(QString("SVMonitor ") + VERSION);
   
	QStringList args = QApplication::arguments();
		
	cng.dirPath = QApplication::applicationDirPath();
	cng.initPath = cng.dirPath + "/svmonitor.ini"; if (args.size() == 2) cng.initPath = args[1];

	initOk_ = init(cng.initPath);
	if (initOk_)
		statusMess(tr("Инициализация параметров успешно"));
	else
	    statusMess(QString(tr("Не найден файл инициализации %1")).arg(cng.initPath));

    auto gp = SV_Graph::createGraphPanel(this, SV_Graph::config(cng.cycleRecMs, cng.packetSz));
    graphPanels_[this] = gp;
    ui.splitter->addWidget(gp);

    if (!initOk_) return;

    Connect();

	load();

	sortSignalByModule();

	// запуск получения данных
	if (cng.com_ena){

		pComReader_ = new SerialPortReader(SerialPortReader::config(cng.com_name, cng.com_speed, cng.cycleRecMs, cng.packetSz));

		if (pComReader_->startServer()) {

			statusMess(QString(tr("Прослушивание %1 порта запущено")).arg(cng.com_name));

			SV_Srv::startServer(srvCng);

			pComReader_->setDataCBack(SV_Srv::receiveData);
		}
		else
			statusMess(QString(tr("%1 порт недоступен")).arg(cng.com_name));
	}
	else{

        if (SV_TcpSrv::runServer(cng.tcp_addr.toStdString(), cng.tcp_port, true)){

            statusMess(QString(tr("Сервер tcp запущен: адрес %1 порт %2").arg(cng.tcp_addr).arg(cng.tcp_port)));

			SV_Srv::startServer(srvCng);

			SV_TcpSrv::setDataCBack(SV_Srv::receiveData);
		}
        else
            statusMess(QString(tr("Не удалось запустить tcp сервер: адрес %1 порт %2").arg(cng.tcp_addr).arg(cng.tcp_port)));
    }

    SV_Script::startUpdateThread(scriptPanel_);
    SV_Trigger::startUpdateThread(triggerPanel_);
}

MainWin::~MainWin(){

	if (!initOk_) return;
	
    SV_Srv::stopServer();

	if (pComReader_ && pComReader_->isRunning())
		pComReader_->stopServer();

	if (db){
		if (!db->saveSignals(SV_Srv::getCopySignalRef()))
			statusMess(tr("Ошибка сохранения сигналов в БД"));
		if (!db->saveTriggers(SV_Trigger::getCopyTriggerRef(triggerPanel_)))
			statusMess(tr("Ошибка сохранения триггеров в БД"));
	}

	writeSettings(cng.initPath);

}

void MainWin::updateGraphSetting(const SV_Graph::graphSetting& gs){

    cng.graphSett = gs;

    for (auto o : graphPanels_)
        SV_Graph::setGraphSetting(o, gs);
}

bool MainWin::eventFilter(QObject *target, QEvent *event){

    if ((event->type() == QEvent::Close) && (target->objectName() == "graphWin")){
             
        graphPanels_.remove(target);
        target->deleteLater();
    }

    return QMainWindow::eventFilter(target, event);
}

void MainWin::sortSignalByModule(){
	 
    int itsz = ui.treeSignals->topLevelItemCount();
    QMap<QString, bool> isExpanded;
    for (int i = 0; i < itsz; ++i)
        isExpanded[ui.treeSignals->topLevelItem(i)->text(0)] = ui.treeSignals->topLevelItem(i)->isExpanded();

	ui.treeSignals->clear();
  
	auto mref = SV_Srv::getCopyModuleRef();

    for (auto& it : mref){

		auto md = it.second;

        if (md->isDelete) continue;

		QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);
		
        if (isExpanded.contains(md->module.c_str()))
           root->setExpanded(isExpanded[md->module.c_str()]);
		root->setFlags(root->flags() | Qt::ItemFlag::ItemIsEditable);
		root->setText(0, md->module.c_str());
        
		md->isEnable ? root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-green.png")):
			root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-yel.png"));
	
		if (!md->isActive) root->setIcon(0, QIcon(":/SVMonitor/images/trafficlight-red.png"));

		QIcon iconImpuls(":/SVMonitor/images/iconImpuls.png");
		QIcon iconSin(":/SVMonitor/images/iconSin.png");

		for (auto& sign : md->signls){
			SV_Cng::signalData* sd = SV_Srv::getSignalData(sign);

			if (sd->isDelete) continue;

			QTreeWidgetItem* item = new QTreeWidgetItem(root);				
			item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
			item->setText(0, sd->name.c_str());
			item->setText(1, SV_Cng::getSVTypeStr(sd->type).c_str());
			item->setText(2, sd->comment.c_str());
			item->setText(3, sd->group.c_str());
			item->setText(4, sign.c_str());
					
			if (sd->type == SV_Cng::valueType::tBool)
				item->setIcon(0, iconImpuls);
			else 
				item->setIcon(0, iconSin);
		}
	}
	
	ui.treeSignals->sortByColumn(1);

	ui.lbAllSignCnt->setText(QString::number(SV_Srv::getCopySignalRef().size()));
        
}

void MainWin::selSignalClick(QTreeWidgetItem* item, int column){

	auto mref = SV_Srv::getCopyModuleRef();

	if (mref.find(item->text(0).toStdString()) != mref.end()){

		ui.lbSignCnt->setText(QString::number(mref[item->text(0).toStdString()]->signls.size()));
	}

}

void MainWin::selSignalDClick(QTreeWidgetItem * item, int column){
	
	auto mref = SV_Srv::getCopyModuleRef();

	if (mref.find(item->text(0).toStdString()) != mref.end()) return;

	if (column > 1)
		ui.treeSignals->editItem(item, column);		
	else{
        SV_Graph::addSignal(graphPanels_[this], item->text(4));
	}

}

void MainWin::selSignalChange(QTreeWidgetItem * item, int column){

	std::string sign = item->text(4).toStdString();
	SV_Cng::signalData* sd = SV_Srv::getSignalData(sign); if (!sd) return;

	switch (column){
		case 2: sd->comment = item->text(2).toStdString(); break;
		case 3: sd->group = item->text(3).toStdString(); break;
	}		
}

void MainWin::contextMenuEvent(QContextMenuEvent * event){

	std::string root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0).toStdString() : "";
	
	if (root.empty() || !qobject_cast<treeWidgetExt*>(focusWidget())) return;

	QMenu* menu = new QMenu(this);

	auto mref = SV_Srv::getCopyModuleRef();

	if (mref.find(root) != mref.end()){
		
		if (mref[root]->isEnable && mref[root]->isActive) menu->addAction(tr("Отключить"));
		else{
			menu->addAction(tr("Включить"));
			menu->addAction(tr("Удалить"));
		}				
	}
	else{
		menu->addAction(tr("Удалить"));
	}

	connect(menu,
		SIGNAL(triggered(QAction*)),
		this,
		SLOT(contextMenuClick(QAction*))
		);

	menu->exec(event->globalPos());
}

void MainWin::contextMenuClick(QAction* act){

	std::string root = ui.treeSignals->currentItem() ? ui.treeSignals->currentItem()->text(0).toStdString() : "";

	if (root.empty()) return;

	auto sref = SV_Srv::getCopySignalRef();

	auto mref = SV_Srv::getCopyModuleRef();

	// signal
	if (mref.find(root) == mref.end()){

		std::string module = ui.treeSignals->currentItem()->parent()->text(0).toStdString();
		std::string sign = root + module;
		if (sref.find(sign) != sref.end()){
			sref[sign]->isDelete = true;

			statusMess(tr("Сигнал удален ") + module.c_str() + ":" + root.c_str());

			sortSignalByModule();
		}
	}
	// module
	else{

		if (act->text() ==  tr("Включить")){

			mref[root]->isEnable = true;
			sortSignalByModule();
		}
		else if (act->text() ==  tr("Отключить")){

			mref[root]->isEnable = false;
			sortSignalByModule();
		}
		else if (act->text() ==  tr("Удалить")){
				
			QMessageBox mb;
			mb.setText(tr("Удалить модуль со всеми сигналами?"));
			mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
			mb.setDefaultButton(QMessageBox::No);

			switch (mb.exec()) {
			case QMessageBox::Yes:
				{
					statusMess(tr("Модуль удален ") + root.c_str());

					mref[root]->isDelete = true;
					
					for (auto sign : sref) if (sign.second->module == root){
						sign.second->isDelete = true;
					}

					sortSignalByModule();
				}
				break;
			case QMessageBox::No:
				break;
			}
		}
	}
		
}

void MainWin::updateTblSignal(){
	
	auto sref = SV_Srv::getCopySignalRef();

	if (sref.size() > SV_VALUE_MAX_CNT)
           statusMess(tr("Превышен лимит количества сигналов: %1. Стабильная работа не гарантирована.").
                    arg(SV_VALUE_MAX_CNT));

	// посмотрим в БД что есть
	if (db){
		
		for (auto& s : sref){

			// только тех, которые еще не видел
			if (!signExist_.contains(s.first.c_str())){

				signalData sd = db->getSignal(s.second->name.c_str(), s.second->module.c_str());
				if (!sd.name.empty()){
					s.second->group = sd.group;
					s.second->comment = sd.comment;
				}

				auto trg = db->getTrigger(s.second->name.c_str(), s.second->module.c_str());
				int sz = trg.size();
				for (int i = 0; i < sz; ++i)
					SV_Trigger::addTrigger(triggerPanel_, trg[i]->name, trg[i]);
					
				signExist_.insert(s.first.c_str());
			}
		}
	}
    
	if (!isSlowMode_)
		sortSignalByModule();
}

void MainWin::updateSignals(){

    for (auto gp : graphPanels_)
        SV_Graph::update(gp);
}

void MainWin::moduleConnect(QString module){
	
	statusMess(tr("Подключен модуль: ") + module);

	auto mref = SV_Srv::getCopyModuleRef();

	if (mref.size() > SV_MODULE_MAX_CNT)
		statusMess(tr("Превышен лимит количества модулей: %1. Стабильная работа не гарантирована.").
		arg(SV_MODULE_MAX_CNT));

	// только тех, которые еще не видел
    if (!signExist_.contains(module)){
				            				
        auto trgOn = db ? db->getTrigger(module + "On") : nullptr;
		if (!trgOn)	{				
            trgOn = new SV_Trigger::triggerData();
            trgOn->name = module + "On";
            trgOn->signal = "";
            trgOn->module = module;
            trgOn->condType = SV_Trigger::eventType::connectModule;
            trgOn->isActive = false;
            trgOn->condValue = 0;
            trgOn->condTOut = 0;
        }
        SV_Trigger::addTrigger(triggerPanel_, module + "On", trgOn);
							
        auto trgOff = db ? db->getTrigger(module + "Off") : nullptr;
        if (!trgOff){
            trgOff = new  SV_Trigger::triggerData();
            trgOff->name = module + "Off";
            trgOff->signal = "";
            trgOff->module = module;
            trgOff->condType = SV_Trigger::eventType::disconnectModule;
            trgOff->isActive = false;
            trgOff->condValue = 0;
            trgOff->condTOut = 0;
        }   
        SV_Trigger::addTrigger(triggerPanel_, module + "Off", trgOff);

        signExist_.insert(module);
	}
	
	sortSignalByModule();
    	
    auto tr = SV_Trigger::getTriggerData(triggerPanel_, module + "On");
    if (tr->isActive)
        tr->condValue = 1;

    tr = SV_Trigger::getTriggerData(triggerPanel_, module + "Off");
    if (tr->isActive)
        tr->condValue = 0;
}

void MainWin::moduleDisconnect(QString module){

    statusMess(tr("Отключен модуль: ") + module);
		
    sortSignalByModule();

    auto tr = SV_Trigger::getTriggerData(triggerPanel_, module + "On");
    if (tr->isActive)
        tr->condValue = 0;

    tr = SV_Trigger::getTriggerData(triggerPanel_, module + "Off");
    if (tr->isActive)
        tr->condValue = 1;
}

void MainWin::onTrigger(QString trigger){
    	
    SV_Trigger::triggerData* td = SV_Trigger::getTriggerData(triggerPanel_, trigger);
		
	QString name = td->module + QString(":") + td->signal + ":" + td->name;

	statusMess(QObject::tr("Событие: ") + name);

	if (db) 
        db->saveEvent(trigger, QDateTime::currentDateTime());

    if (!td->userProcPath.isEmpty()){
        QFile f(td->userProcPath);
		if (f.exists()){

            QStringList args = td->userProcArgs.split('\t'); for (auto& ar : args) ar = ar.trimmed();

            QProcess::startDetached(td->userProcPath, args);

            statusMess(name + QObject::tr(" Процесс запущен: ") + td->userProcPath + " args: " + td->userProcArgs);
		}
		else
            statusMess(name + QObject::tr(" Путь не найден: ") + td->userProcPath);
	}
}

void MainWin::initTrayIcon(){

	trayIcon_ = new QSystemTrayIcon(this);
	QIcon trayImage(":/SVMonitor/images/logo.png");
	trayIcon_->setIcon(trayImage);

	// Setting system tray's icon menu...
	QMenu* trayIconMenu = new QMenu(this);

	QAction* restoreAction = new QAction("Восстановить", trayIcon_);
	QAction* quitAction = new QAction("Выход", trayIcon_);

	connect(restoreAction, SIGNAL(triggered()), this, SLOT(showMaximized()));
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

	trayIconMenu->addAction(restoreAction);
	trayIconMenu->addAction(quitAction);

	trayIcon_->setContextMenu(trayIconMenu);

	connect(trayIcon_, &QSystemTrayIcon::activated, [this](){
		trayIcon_->hide();
		this->showMaximized();

		isSlowMode_ = false;

		sortSignalByModule();
	});
}

void MainWin::slowMode(){

	isSlowMode_ = true;
		
	ui.treeSignals->clear();

	this->trayIcon_->show();
	this->hide();

}

MainWin::config MainWin::getConfig(){

	return cng;
}

void MainWin::updateConfig(MainWin::config cng_){

    cng = cng_;

	srvCng.outArchiveEna  = cng.outArchiveEna;
	srvCng.outArchiveHourCnt = cng.outArchiveHourCnt;
	srvCng.outArchiveName = cng.outArchiveName.toStdString();
	srvCng.outArchivePath = cng.outArchivePath.toStdString();

	SV_Srv::setConfig(srvCng);
}

QVector<uEvent> MainWin::getEvents(QDateTime bg, QDateTime en){

	return db ? db->getEvents(bg, en) : QVector<uEvent>();
}

QDialog* MainWin::addNewWindow(const QRect& pos){

    QDialog* graphWin = new QDialog(this, Qt::Window);
    graphWin->setObjectName("graphWin");
    graphWin->installEventFilter(this);

    QVBoxLayout* vertLayout = new QVBoxLayout(graphWin);
    vertLayout->setSpacing(0);
    vertLayout->setContentsMargins(5, 5, 5, 5);

    SV_Graph::config config(cng.cycleRecMs, cng.packetSz);
    config.isShowTable = false;

    auto gp = SV_Graph::createGraphPanel(graphWin, config);
    SV_Graph::setLoadSignalData(gp, [](const QString& sign){
        return SV_Srv::signalBufferEna(sign.toUtf8().data());
    });
    SV_Graph::setGetCopySignalRef(gp, getCopySignalRefSrv);
    SV_Graph::setGetSignalData(gp, getSignalDataSrv);
    SV_Graph::setGraphSetting(gp, cng.graphSett);

    graphPanels_[graphWin] = gp;
    vertLayout->addWidget(gp);
         
    graphWin->show();  

    if (!pos.isNull()){
        graphWin->setGeometry(pos);
        graphWin->resize(QSize(pos.width(), pos.height()));
    }

    return graphWin;
}