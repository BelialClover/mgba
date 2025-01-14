/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QQueue>
#include <QWidget>

#include "ui_LogView.h"

namespace QGBA {

class LogController;
class Window;

class LogView : public QWidget {
Q_OBJECT

public:
	LogView(LogController* log, Window* window, QWidget* parent = nullptr);

signals:
	void levelsEnabled(int levels);
	void levelsDisabled(int levels);

public slots:
	void playCry(const QString& number);
	void playAnimeCry(const QString& number);
	int  insertCompressedPokemon(char const* species, int level, int shiny, char const* nickname, int value1, int value2, int value3, int value4, int value5, int value6, int value7, int value8, int value9);
	int  insertString(char const* filename);
	void postLog(int level, int category, const QString& log);
	void setLevels(int levels);
	void clear();

private slots:
	void setMaxLines(int);

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	static const int DEFAULT_LINE_LIMIT = 1000;

	Ui::LogView m_ui;
	int m_lines = 0;
	int m_lineLimit = DEFAULT_LINE_LIMIT;
	Window* m_window;
	QQueue<QString> m_pendingLines;

	void setLevel(int level, bool);

	void clearLine();
};

}
