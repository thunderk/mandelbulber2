/**
 * Mandelbulber v2, a 3D fractal generator
 *
 * cHeadless - class to handle CLI instructions without GUI manipulation
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
 * Authors: Krzysztof Marczak (buddhi1980@gmail.com), Sebastian Jennen
 */

#include "headless.h"
#include "cimage.hpp"
#include "files.h"
#include "render_job.hpp"
#include "global_data.hpp"
#include "interface.hpp"

cHeadless::cHeadless() : QObject()
{
	// TODO Auto-generated constructor stub

}

cHeadless::~cHeadless()
{
	// TODO Auto-generated destructor stub
}

void cHeadless::RenderStillImage(QString filename, QString imageFileFormat)
{
	cImage *image = new cImage(gPar->Get<int>("image_width"), gPar->Get<int>("image_height"));
	cRenderJob *renderJob = new cRenderJob(gPar, gParFractal, image, &gMainInterface->stopRequest);

	cRenderingConfiguration config;
	config.EnableConsoleOutput();
	config.DisableRefresh();
	config.DisableProgressiveRender();
	config.EnableNetRender();

	renderJob->Init(cRenderJob::still, config);
	renderJob->Execute();

	//TODO saving in different image formats

	if(imageFileFormat == "jpg")
	{
		SaveImage(filename, IMAGE_FILE_TYPE_JPG, image);
	}
	else if(imageFileFormat == "png16")
	{
		SaveImage(filename, IMAGE_FILE_TYPE_PNG, image);
	}
	else if(imageFileFormat == "png16")
	{
		structSaveImageChannel saveImageChannel(IMAGE_CONTENT_COLOR, IMAGE_CHANNEL_QUALITY_16, "");
		SavePNG(filename, image, saveImageChannel, false);
	}
	else if(imageFileFormat == "png16alpha")
	{
		structSaveImageChannel saveImageChannel(IMAGE_CONTENT_COLOR, IMAGE_CHANNEL_QUALITY_16, "");
		SavePNG(filename, image, saveImageChannel, true);
	}

	delete renderJob;
	delete image;
	emit finished();
}

void cHeadless::RenderFlightAnimation()
{
	cImage *image = new cImage(gPar->Get<int>("image_width"), gPar->Get<int>("image_height"));
	gFlightAnimation = new cFlightAnimation(gMainInterface, gAnimFrames, image, NULL);
	return gFlightAnimation->slotRenderFlight();
}

void cHeadless::slotNetRender()
{
	cImage *image = new cImage(gPar->Get<int>("image_width"), gPar->Get<int>("image_height"));
	cRenderJob *renderJob = new cRenderJob(gPar, gParFractal, image, &gMainInterface->stopRequest);

	cRenderingConfiguration config;
	config.EnableConsoleOutput();
	config.DisableRefresh();
	config.DisableProgressiveRender();
	config.EnableNetRender();

	renderJob->Init(cRenderJob::still, config);
	renderJob->Execute();

	delete renderJob;
	delete image;
	emit finished();
}

void cHeadless::RenderingProgressOutput(const QString &header, const QString &progressTxt, double percentDone, bool newLine)
{
	QTextStream out(stdout);
	QString formatedText = formatLine(progressTxt) + " ";
	QString text;
	if(systemData.terminalWidth > 0)
	{
		int freeWidth = systemData.terminalWidth - progressTxt.length() - header.length() - 6;
		int intProgress = freeWidth * percentDone;
		text = "\r";
		text += colorize(header + ": ", ansiYellow, noExplicitColor, true);
		text += formatedText;
		text += colorize("[", ansiBlue, noExplicitColor, true);
		text += colorize(QString(intProgress, '#'), ansiMagenta, noExplicitColor, true);
		text += QString(freeWidth - intProgress, ' ');
		text += colorize("]", ansiBlue, noExplicitColor, true);
		if(newLine) text += "\n";
	}
	else
	{
		text += QString("\n");
	}
	out << text;
	out.flush();
}

QString cHeadless::colorize(QString text, ansiColor foregroundcolor, ansiColor backgroundColor, bool bold)
{
	// more info on ANSI escape codes here: https://en.wikipedia.org/wiki/ANSI_escape_code
#ifdef WIN32 /* WINDOWS */
	return text;
#else
	if(!systemData.useColor) return text;

	QStringList ansiSequence;
	if(foregroundcolor != noExplicitColor) ansiSequence << QString::number(foregroundcolor + 30);
	if(backgroundColor != noExplicitColor) ansiSequence << QString::number(backgroundColor + 40);
	if(bold) ansiSequence << "1";

	if(ansiSequence.size() == 0) return text;

	QString colorizedString = "\033["; // start ANSI escape sequence
	colorizedString += ansiSequence.join(";");
	colorizedString += "m"; // end ANSI escape sequence
	colorizedString += text;
	colorizedString += "\033[0m"; // reset color and bold after string
	return colorizedString;
#endif
}

QString cHeadless::formatLine(const QString& text)
{
#ifdef WIN32 /* WINDOWS */
	return text;
#else
	if(!systemData.useColor) return text;
	QList<QRegularExpression> reType;
	reType.append(QRegularExpression("^(Done )(.*?)(, )(elapsed: )(.*?)(, )(estimated to end: )(.*)"));
	reType.append(QRegularExpression("^(Gotowe )(.*?)(, )(upłynęło: )(.*?)(, )(do końca: )(.*)"));
	reType.append(QRegularExpression("^(Fortschritt )(.*?)(, )(vergangen: )(.*?)(, )(voraussichtlich noch: )(.*)"));

	reType.append(QRegularExpression("^(.*?)( Done)(, )(total time: )(.*)"));
	reType.append(QRegularExpression("^(.*?)( gotowe)(, )(całkowity czas: )(.*)"));
	reType.append(QRegularExpression("^(.*?)( Fertig)(, )(Gesamtzeit: )(.*)"));

	QRegularExpressionMatch matchType;
	for(int i = 0; i < reType.size(); i++){
		matchType = reType.at(i).match(text);
		if (matchType.hasMatch()) break;
	}

	if (!matchType.hasMatch())
	{
		return text;
	}

	QString out = "";
	out += colorize(matchType.captured(1), noExplicitColor, noExplicitColor, false);
	out += colorize(matchType.captured(2), noExplicitColor, noExplicitColor, true);
	out += colorize(matchType.captured(3), noExplicitColor, noExplicitColor, false);

	out += colorize(matchType.captured(4), ansiGreen, noExplicitColor, false);
	out += colorize(matchType.captured(5), ansiGreen, noExplicitColor, true);

	if(matchType.lastCapturedIndex() == 8)
	{
		out += colorize(matchType.captured(6), noExplicitColor, noExplicitColor, false);
		out += colorize(matchType.captured(7), ansiCyan, noExplicitColor, false);
		out += colorize(matchType.captured(8), ansiCyan, noExplicitColor, true);
	}

	return out;
#endif
}
