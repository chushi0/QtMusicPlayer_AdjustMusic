#pragma once

#include <QDialog>

namespace Ui {
class RawTextDialog;
}

class RawTextDialog : public QDialog
{
   Q_OBJECT

public:
   explicit RawTextDialog(QWidget *parent = nullptr);
   ~RawTextDialog();
   void setString(const QString& string);
   QString string();

private:
   Ui::RawTextDialog *ui;
};
