#include "rawtextdialog.h"
#include "ui_rawtextdialog.h"

RawTextDialog::RawTextDialog(QWidget *parent) :
   QDialog(parent),
   ui(new Ui::RawTextDialog)
{
   ui->setupUi(this);
}


RawTextDialog::~RawTextDialog()
{
   delete ui;
}


void RawTextDialog::setString(const QString& string)
{
   ui->textEdit->setPlainText(string);
}


QString RawTextDialog::string()
{
   return ui->textEdit->toPlainText();
}
