#include "mainwindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>

#include <windows.h>
#include <winbase.h>

static MainWindow *w;

long __stdcall callback(_EXCEPTION_POINTERS *)
{
   QString file = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/autosave.lrc";
   QFile   f(file);

   w->lyric >> f;
   QApplication::beep();
   QMessageBox::critical(nullptr, "发生错误", QString("很抱歉，程序遇到错误需要退出。\n\n您正在编辑的内容已自动保存到%1，您可以稍后恢复继续编辑。").arg(file));
   return EXCEPTION_EXECUTE_HANDLER;
}


int main(int argc, char *argv[])
{
   SetUnhandledExceptionFilter(callback);

   QApplication a(argc, argv);
   QString      musicFile;
   QString      lyricFile;

   // 如果没有选择音乐，要求用户选择音乐
   if (argc < 2)
   {
      musicFile = QFileDialog::getOpenFileName(nullptr, "选择音乐文件", QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                               "支持的音乐格式 (*.wav *.mp3 *.ogg *.flac *.aac *.m4a);;全部文件 (*.*)");
      if (musicFile.isNull())
      {
         return 0;
      }
   }
   else
   {
      musicFile = a.arguments()[1];
   }

   // 如果没有选择歌词，要求用户选择歌词
   if (argc < 3)
   {
      lyricFile = QFileDialog::getOpenFileName(nullptr, "选择歌词文件", QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                               "标准歌词文件 (*.lrc);;增强歌词文件 (*.lrc);;纯文本文件 (*.txt);;全部文件 (*.*)");
      if (lyricFile.isNull())
      {
         return 0;
      }
   }
   else
   {
      lyricFile = a.arguments()[2];
   }
   w = new MainWindow(musicFile, lyricFile);

   w->show();

   int res = a.exec();
   delete w;
   return res;
}
