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
#include <QPrinter>
#include <QPrintDialog>
#include "forms/mainWin.h"
#include "SVGraphPanel/SVGraphPanel.h"
#include "SVStatPanel/SVStatPanel.h"
#include "SVConfig/SVConfigLimits.h"
#include "SVConfig/SVConfigData.h"


const QString VERSION = "1.0.0.3";
MainWin* mainWin = nullptr;

using namespace SV_Cng;


bool loadSignalData(const QString& sign);

QMap<QString, signalData*> getCopySignalRef(){

 //   mainWin->mtx_.lock();

    auto sref = mainWin->signalRef_;

 //   mainWin->mtx_.unlock();

    return sref;
}

signalData* getSignalData(const QString& sign){

   // mainWin->mtx_.lock();

    signalData* sd = mainWin->signalRef_.contains(sign) ? mainWin->signalRef_[sign] : nullptr;

  //  mainWin->mtx_.unlock();

    return sd;
}

bool MainWin::writeSettings(QString pathIni){

    QFile file(pathIni);

    QTextStream txtStream(&file);	QStringList sList;

    file.open(QIODevice::WriteOnly);
    txtStream << "[Param]" << endl;
    txtStream << endl;
    txtStream << "selOpenDir = " << cng.selOpenDir << endl;
    txtStream << endl;
    txtStream << "cycleRecMs = " << cng.cycleRecMs << endl;
    txtStream << "packetSz = " << cng.packetSz << endl;
    txtStream << endl;
    txtStream << "sortByMod = " << (cng.sortByMod ? 1 : 0) << endl;
    txtStream << endl;
	file.close();

    return true;
}

bool MainWin::readSignals(QString path){

    QFile file(path);

    QTextStream txtStream(&file);

    if (file.open(QIODevice::ReadOnly)){

        while (!txtStream.atEnd()){

            QStringList lst = txtStream.readLine().split("\t");

            if (lst.size() >= 4){

				std::string module = qPrintable(lst[0]);
				std::string sname = qPrintable(lst[1]);
                std::string sign = sname + module;

                if (signalRef_.contains(sign.c_str())) continue;

                if (!moduleRef_.contains(module.c_str()))
                    moduleRef_[module.c_str()] = new moduleData(module);

                moduleRef_[module.c_str()]->signls.push_back(sign);
                moduleRef_[module.c_str()]->isActive = false;

				std::string group = (lst.size() == 5) ? qPrintable(lst[4]) : "";
				std::string comment = qPrintable(lst[3]);
				std::string stype = qPrintable(lst[2]);


                if (!groupRef_.contains(group.c_str()))
                    groupRef_[group.c_str()] = new groupData(group.c_str());

                groupRef_[group.c_str()]->signls.push_back(sign);
                groupRef_[group.c_str()]->isActive = false;

                auto sd = new signalData();
                sd->name = sname;
                sd->module = module;
                sd->group = group;
                sd->comment = comment;
                sd->type = getSVType(stype);

                signalRef_[sign.c_str()] = sd;
            }
        }

        file.close();

    }

    return true;
}

bool MainWin::writeSignals(QString path){

    QFile file(path);

    QTextStream txtStream(&file);

    file.open(QIODevice::WriteOnly);

    for (auto s : signalRef_){

        txtStream << s->module.c_str() << '\t'
                  << QString::fromLocal8Bit(s->name.c_str()) << '\t'
                  << getSVTypeStr(s->type).c_str() << '\t'
                  << QString::fromLocal8Bit(s->comment.c_str()) << '\t'
                  << QString::fromLocal8Bit(s->group.c_str()) << endl;
    }

    file.close();

    return true;
}

void MainWin::updateGroup(QString group, QString sign){


	if (!groupRef_.contains(group))
		groupRef_[group] = new groupData(group.toUtf8().data());

	std::string sname = sign.toUtf8().data();

	bool isSingExist = false;
	for (auto& s : groupRef_[group]->signls){
	    if (s == sname){
            isSingExist = true;
            break;
        }
	}

	if (!isSingExist)
		groupRef_[group]->signls.push_back(sname);

}

void MainWin::load(){

	ui.treeSignals->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.treeSignals->setIconSize(QSize(40, 20));

	//exportWin_ = new exportWin(this); exportWin_->setWindowFlags(Qt::Window);

	graphPanel_ = SV_Graph::createGraphPanel(this, SV_Graph::config(cng.cycleRecMs,cng.packetSz, SV_Graph::modeGr::viewer));

    SV_Graph::setGetCopySignalRef(graphPanel_, getCopySignalRef);
    SV_Graph::setGetSignalData(graphPanel_, getSignalData);
    SV_Graph::setLoadSignalData(graphPanel_, loadSignalData);

	statPanel_ = SV_Stat::createStatPanel(this, SV_Stat::config(cng.cycleRecMs,cng.packetSz));
	statPanel_->setWindowFlags(Qt::Window);
	SV_Stat::setGetCopySignalRef(statPanel_, getCopySignalRef);
	SV_Stat::setGetSignalData(statPanel_, getSignalData);
	SV_Stat::setLoadSignalData(statPanel_, loadSignalData);
	SV_Stat::setSetTimeInterval(statPanel_, [](qint64 st, qint64 en){
		SV_Graph::setTimeInterval(mainWin->graphPanel_, st,en);
	});
	SV_Stat::setGetTimeInterval(statPanel_, [](){
		return SV_Graph::getTimeInterval(mainWin->graphPanel_);
	});

	ui.splitter->addWidget(graphPanel_);
	ui.progressBar->setVisible(false);

	ui.btnSortByModule->setChecked(cng.sortByMod);
	ui.btnSortByGroup->setChecked(!cng.sortByMod);

	readSignals(QApplication::applicationDirPath() + "/svsignals.txt");
}

void MainWin::Connect(){

	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(actionOpenData()));
	connect(ui.actionStat, SIGNAL(triggered()), this, SLOT(actionOpenStat()));

	connect(ui.treeSignals, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(selSignalClick(QTreeWidgetItem*, int)));
	connect(ui.treeSignals, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(selSignalDClick(QTreeWidgetItem*, int)));
	connect(ui.treeSignals, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(selSignalChange(QTreeWidgetItem*, int)));

	connect(ui.actionExit, &QAction::triggered, [this]() { 
		this->close();
	});

	connect(ui.actionExport, &QAction::triggered, [this]() {
		if (exportWin_) exportWin_->show();
	});
		
	connect(ui.btnSortByGroup, &QPushButton::clicked, [this]() {
		this->ui.btnSortByGroup->setChecked(true);
		this->ui.btnSortByModule->setChecked(false);
		cng.sortByMod = false;
		sortSignalByGroupOrModule(false);
	});

	connect(ui.btnSortByModule, &QPushButton::clicked, [this]() {
		this->ui.btnSortByModule->setChecked(true);
		this->ui.btnSortByGroup->setChecked(false);
		cng.sortByMod = true;
		sortSignalByGroupOrModule(true);
	});

	connect(ui.actionPrint, &QAction::triggered, [this]() {

		QPrinter printer(QPrinter::HighResolution);
		printer.setPageMargins(12, 16, 12, 20, QPrinter::Millimeter);
		printer.setFullPage(false);

		QPrintDialog printDialog(&printer, this);
		if (printDialog.exec() == QDialog::Accepted) {

			QPainter painter(&printer);

			double xscale = printer.pageRect().width() / double(graphPanel_->width());
			double yscale = printer.pageRect().height() / double(graphPanel_->height());
			double scale = qMin(xscale, yscale);
			painter.translate(printer.paperRect().x(), printer.paperRect().y());
			painter.scale(scale, scale);

			graphPanel_->render(&painter);
		}
	});


	connect(ui.actionProgram, &QAction::triggered, [this]() {
	QMessageBox::about(this, tr("About SVisual"),
			tr("<h2>SVViewer </h2>"
			"<p>Программное обеспечение предназначенное"
			"<p>для анализа сигналов с устройст."
            "<p>2017"));
	});
}

bool MainWin::init(QString initPath){


	QSettings settings(initPath, QSettings::IniFormat);
	settings.beginGroup("Param");

	cng.cycleRecMs =  settings.value("cycleRecMs", 100).toInt();
	cng.cycleRecMs = qMax(cng.cycleRecMs, 10);
	cng.packetSz = settings.value("packetSz", 10).toInt();
	cng.packetSz = qMax(cng.packetSz, 1);

	cng.selOpenDir = settings.value("selOpenDir", "").toString();
	cng.sortByMod = settings.value("sortByMod", 1).toInt() == 1;
		
	if (!QFile(initPath).exists())
		writeSettings(initPath);

	return true;

}

MainWin::MainWin(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	mainWin = this;

	this->setWindowTitle(QString("SVViewer ") + VERSION);

	QStringList args = QApplication::arguments();
	cng.initPath = QApplication::applicationDirPath(); if (args.size() == 2) cng.initPath = args[1];
	init(cng.initPath + "/sviewer.ini");

	Connect();

	load();
}

MainWin::~MainWin()
{	
	writeSettings(cng.initPath + "/sviewer.ini");
	writeSignals(cng.initPath + "/svsignals.txt");
}

void MainWin::sortSignalByGroupOrModule(bool byModule){

	ui.treeSignals->clear();
	auto sref = getCopySignalRef();

	QIcon iconImpuls(":/SVViewer/images/iconImpuls.png");
	QIcon iconSin(":/SVViewer/images/iconSin.png");

	int scnt = 0;
	if (byModule){

		ui.treeSignals->headerItem()->setText(2, tr("Группа"));
		
		for (auto itMod : moduleRef_){

			if (!itMod->isActive) continue;

			QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);
			
			root->setText(0, itMod->module.c_str());
			for (auto& s : itMod->signls){

				QString sname = s.c_str();

				if (!sref[sname]->isActive) continue;
				++scnt;
				
				QTreeWidgetItem* item = new QTreeWidgetItem(root);
				item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
				item->setText(0, sref[sname]->name.c_str());
				item->setText(1, SV_Cng::getSVTypeStr(sref[sname]->type).c_str());
				item->setText(2, sref[sname]->group.c_str());
				item->setText(3, sref[sname]->comment.c_str());
				item->setText(4, sname);

				if (sref[sname]->type == valueType::tBool)
					item->setIcon(0, iconImpuls);
				else
					item->setIcon(0, iconSin);
			}
		}
	}
	else {

		ui.treeSignals->headerItem()->setText(2, tr("Модуль"));

		for (auto itGrp : groupRef_){

			if (!itGrp->isActive) continue;

			if (itGrp->signls.empty()) continue;

			QTreeWidgetItem* root = new QTreeWidgetItem(ui.treeSignals);

			root->setText(0, itGrp->group.c_str());
					
			for (auto& s : itGrp->signls){

				QString sname = s.c_str();

				if (!sref[sname]->isActive) continue;
				++scnt;

				QTreeWidgetItem* item = new QTreeWidgetItem(root);
				item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
				item->setText(0, sref[sname]->name.c_str());
				item->setText(1, SV_Cng::getSVTypeStr(sref[sname]->type).c_str());
				item->setText(2, sref[sname]->module.c_str());
				item->setText(3, sref[sname]->comment.c_str());
				item->setText(4, sname);

				if (sref[sname]->type == valueType::tBool)
					item->setIcon(0, iconImpuls);
				else
					item->setIcon(0, iconSin);
			}
		}
	}

	ui.treeSignals->sortByColumn(1);

	ui.lbAllSignCnt->setText(QString::number(scnt));
	
}

void MainWin::loadDataFinished(bool ok){

	if (ok){
		ui.lbStatusMess->setText(cng.selOpenDir);
				
		sortSignalByGroupOrModule(ui.btnSortByModule->isChecked());
	}
	else
		ui.lbStatusMess->setText(tr("Файл не удалось прочитать"));
			
	ui.progressBar->setVisible(false);

	thrLoadData_->deleteLater();
	
}

void MainWin::actionOpenData(){
	
	QStringList files = QFileDialog::getOpenFileNames(this,
		tr("Добавление файлов данных"), cng.selOpenDir,
		"dat files (*.dat)");

	if (files.isEmpty()) return;

	ui.progressBar->setVisible(true);

	thrLoadData_ = new thrLoadData(files);
	connect(thrLoadData_, SIGNAL(finished(bool)), this, SLOT(loadDataFinished(bool)));
}

void MainWin::actionOpenStat(){
		
	statPanel_->show();
}

void MainWin::selSignalClick(QTreeWidgetItem* item, int column){

	if (moduleRef_.contains(item->text(0))){
				
		auto sref = getCopySignalRef();
		std::string module = item->text(0).toUtf8().data();
		int scnt = 0;
		for (auto s : sref)						
			if ((s->module == module) && s->isActive) ++scnt;
		
		ui.lbSignCnt->setText(QString::number(scnt));
	}
	
}

void MainWin::selSignalDClick(QTreeWidgetItem * item, int column){
	
	if (moduleRef_.contains(item->text(0))) return;

	if ((column > 1) && (cng.sortByMod || (column != 2)))
		ui.treeSignals->editItem(item, column);
	else
		SV_Graph::addSignal(graphPanel_, item->text(4));
}

void MainWin::selSignalChange(QTreeWidgetItem * item, int column){

	QString sign = item->text(4);
	signalData* sd = getSignalData(sign); if (!sd) return;

	switch (column){
		case 2:
			if (cng.sortByMod){
			   sd->group = item->text(2).toUtf8().data();
			   updateGroup(item->text(2), sign);
		    }
		    break;
		case 3: sd->comment = item->text(3).toUtf8().data(); break;
	}
		
}
