#include "pch.h"
#include "logtimewindow.h"
#include "ui_logtimewindow.h"

#include <QAbstractItemModel>
#include <QStringListModel>
#include <QCloseEvent>
#include <QClipboard>

LogTimeWindow::LogTimeWindow(QWidget *parent) :
   QMainWindow(parent),
   ui(new Ui::LogTimeWindow)
{
   ui->setupUi(this);
   setWindowTitle("时间记录");
   connect(ui->listView, &QListView::activated, [this] {
      QModelIndex currentIndex = ui->listView->currentIndex();
      int index = currentIndex.row();
      QStringListModel *model = dynamic_cast<QStringListModel *>(ui->listView->model());
      QString time            = model->stringList()[index];
      QClipboard *clipboard   = QApplication::clipboard();
      clipboard->setText(time);
   });
}


LogTimeWindow::~LogTimeWindow()
{
   delete ui;
}


void LogTimeWindow::updateList(const QList<long>& list)
{
   QAbstractItemModel *model = ui->listView->model();

   QStringList stringList;

   for (long e:list)
   {
      stringList << readableTimeString(e);
   }

   if (model)
   {
      model->deleteLater();
   }
   ui->listView->setModel(new QStringListModel(stringList));
}


void LogTimeWindow::closeEvent(QCloseEvent *event)
{
   event->ignore();
   hide();
}
