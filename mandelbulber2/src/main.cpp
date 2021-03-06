/**
 * Mandelbulber v2, a 3D fractal generator
 *
 * main function - 'main' function for application
 *
 * Copyright (C) 2014 Krzysztof Marczak
 *
 * This file is part of Mandelbulber.
 *
 * Mandelbulber is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Mandelbulber is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should have received a copy of the GNU
 * General Public License along with Mandelbulber. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Krzysztof Marczak (buddhi1980@gmail.com)
 */

#include "main.hpp"
#include "system.hpp"
#include "fractparams.hpp"
#include "interface.hpp"
#include "initparameters.hpp"
#include "fractal_list.hpp"
#include "undo.h"
#include "global_data.hpp"
#include "settings.hpp"
#include "command_line_interface.hpp"
#include "headless.h"
#include "error_message.hpp"

#include <qapplication.h>

int main(int argc, char *argv[])
{
	//Initialization of system functions
	InitSystem();

	//configure debug output
	qInstallMessageHandler(myMessageOutput);

	//class for interface windows
	gMainInterface = new cInterface;

	WriteLog("Prepare QApplication");
	QCoreApplication *gCoreApplication = new QCoreApplication(argc, argv);
	gCoreApplication->setOrganizationName("Mandelbulber");
	gCoreApplication->setApplicationName("Mandelbulber");
	gCoreApplication->setApplicationVersion(MANDELBULBER_VERSION_STRING);

	UpdateLanguage(gCoreApplication);

	cCommandLineInterface commandLineInterface(gApplication);

	if (commandLineInterface.isNoGUI())
	{
		gApplication = qobject_cast<QApplication *>(gCoreApplication);
	}
	else
	{
		delete gCoreApplication;
		gApplication = new QApplication(argc, argv);
		gApplication->setOrganizationName("Mandelbulber");
		gApplication->setApplicationName("Mandelbulber");
		gApplication->setApplicationVersion(MANDELBULBER_VERSION_STRING);
	}

	//registering types for queued connections
	qRegisterMetaType<cStatistics>("cStatistics");
	qRegisterMetaType<QList<QByteArray> >("QList<QByteArray>");
	qRegisterMetaType<QList<int> >("QList<int>");
	qRegisterMetaType<cParameterContainer>("cParameterContainer");
	qRegisterMetaType<cFractalContainer>("cFractalContainer");
	qRegisterMetaType<sTextures>("sTextures");
	qRegisterMetaType<cProgressText::enumProgressType>("cProgressText::enumProgressType");
	qRegisterMetaType<QVector<int> >("QVector<int>");
	qRegisterMetaType<CVector2<double> >("CVector2<double>");
	qRegisterMetaType<QMessageBox::StandardButtons>("QMessageBox::StandardButtons");
	qRegisterMetaType<QMessageBox::StandardButtons*>("QMessageBox::StandardButtons*");
	qRegisterMetaType<cErrorMessage::enumMessageType>("cErrorMessage::enumMessageType");

	gErrorMessage = new cErrorMessage;

	//create default directories and copy all needed files
	WriteLog("CreateDefaultFolders()");
	if (!CreateDefaultFolders())
	{
		qCritical() << "Files/directories initialization failed" << endl;
		return 73;
	}

	//create internal database with parameters
	gPar = new cParameterContainer;
	gParFractal = new cFractalContainer;

	//Allocate container for animation frames
	gAnimFrames = new cAnimationFrames;

	//Allocate container for key frames
	gKeyframes = new cKeyframes;

	gPar->SetContainerName("main");
	InitParams(gPar);
	for (int i = 0; i < NUMBER_OF_FRACTALS; i++)
	{
		gParFractal->at(i).SetContainerName(QString("fractal") + QString::number(i));
		InitFractalParams(&gParFractal->at(i));
	}
	//Define list of fractal formulas
	DefineFractalList(&fractalList);

	//Netrender
	gNetRender = new CNetRender(systemData.numberOfThreads);

	//loading AppSettings
	if (QFile(systemData.dataDirectory + "mandelbulber.ini").exists())
	{
		cSettings parSettings(cSettings::formatAppSettings);
		parSettings.LoadFromFile(systemData.dataDirectory + "mandelbulber.ini");
		parSettings.Decode(gPar, gParFractal);
	}

	UpdateDefaultPaths();
	if (!commandLineInterface.isNoGUI())
	{
		UpdateUIStyle();
		UpdateUISkin();
	}
	UpdateLanguage(gApplication);

	commandLineInterface.ReadCLI();

	if (!commandLineInterface.isNoGUI())
	{
		gMainInterface->ShowUi();
		gFlightAnimation = new cFlightAnimation(gMainInterface,
																						gAnimFrames,
																						gMainInterface->mainImage,
																						gMainInterface->renderedImage,
																						gPar,
																						gParFractal,
																						gMainInterface->mainWindow);
		gKeyframeAnimation = new cKeyframeAnimation(gMainInterface,
																								gKeyframes,
																								gMainInterface->mainImage,
																								gMainInterface->renderedImage,
																								gPar,
																								gParFractal,
																								gMainInterface->mainWindow);

		QObject::connect(gFlightAnimation,
										 SIGNAL(updateProgressAndStatus(const QString&, const QString&, double, cProgressText::enumProgressType)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateProgressAndStatus(const QString&, const QString&, double, cProgressText::enumProgressType)));
		QObject::connect(gFlightAnimation,
										 SIGNAL(updateProgressHide(cProgressText::enumProgressType)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateProgressHide(cProgressText::enumProgressType)));
		QObject::connect(gFlightAnimation,
										 SIGNAL(updateStatistics(cStatistics)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateStatistics(cStatistics)));
		QObject::connect(gKeyframeAnimation,
										 SIGNAL(updateProgressAndStatus(const QString&, const QString&, double, cProgressText::enumProgressType)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateProgressAndStatus(const QString&, const QString&, double, cProgressText::enumProgressType)));
		QObject::connect(gKeyframeAnimation,
										 SIGNAL(updateProgressHide(cProgressText::enumProgressType)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateProgressHide(cProgressText::enumProgressType)));
		QObject::connect(gKeyframeAnimation,
										 SIGNAL(updateStatistics(cStatistics)),
										 gMainInterface->mainWindow,
										 SLOT(slotUpdateStatistics(cStatistics)));

		try
		{
			gQueue = new cQueue(gMainInterface,
													systemData.dataDirectory + "queue.fractlist",
													systemData.dataDirectory + "queue",
													gMainInterface->mainWindow);
		} catch (QString &ex)
		{
			cErrorMessage::showMessage(QObject::tr("Cannot init queue: ") + ex,
																 cErrorMessage::errorMessage);
			return -1;
		}
	}

	//write parameters to ui
	if (!commandLineInterface.isNoGUI())
	{
		gMainInterface->ComboMouseClickUpdate();
		gMainInterface->SynchronizeInterface(gPar, gParFractal, cInterface::write);
		gMainInterface->ComboMouseClickUpdate();

		gMainInterface->AutoRecovery();
	}

	gMainInterface->interfaceReady = true;

	commandLineInterface.ProcessCLI();

	//start main Qt loop
	WriteLog("application->exec()");
	int result = 0;
	if (!commandLineInterface.isNoGUI()) result = gApplication->exec();

	//clean objects when exit
	delete gPar;
	gPar = NULL;
	delete gParFractal;
	if (gFlightAnimation) delete gFlightAnimation;
	if (gKeyframeAnimation) delete gKeyframeAnimation;
	delete gAnimFrames;
	delete gKeyframes;
	delete gNetRender;
	delete gQueue;
	delete gMainInterface;
	delete gErrorMessage;
	delete gApplication;
	return result;
}

