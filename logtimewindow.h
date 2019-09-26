#pragma once

#include <QMainWindow>

namespace Ui {
class LogTimeWindow;
}

class LogTimeWindow : public QMainWindow
{
   Q_OBJECT

public:
   explicit LogTimeWindow(QWidget *parent = nullptr);
   ~LogTimeWindow();
   void updateList(const QList<long>& list);
   void closeEvent(QCloseEvent *event);

private:
   Ui::LogTimeWindow *ui;
};
