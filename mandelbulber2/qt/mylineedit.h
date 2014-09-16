/*
 * mylineedit.hpp
 *
 *  Created on: May 10, 2014
 *      Author: krzysztof
 */

#ifndef MYLINEEDIT_HPP_
#define MYLINEEDIT_HPP_

#include <QtGui>
#include <QtCore>
#include <QLineEdit>
#include <QMenu>
#include "../src/parameters.hpp"

class MyLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	MyLineEdit(QWidget *parent = 0)  : QLineEdit(parent)
	{
		actionResetToDefault = NULL;
		parameterContainer = NULL;
	};

	void AssignParameterContainer(cParameterContainer *container) {parameterContainer = container;}
	void AssingParameterName(QString name) {parameterName = name;}

private:
	QAction *actionResetToDefault;
	QString GetType(const QString &name);
	cParameterContainer *parameterContainer;
	QString parameterName;

protected:
	void contextMenuEvent(QContextMenuEvent *event);
};



#endif /* MYLINEEDIT_HPP_ */