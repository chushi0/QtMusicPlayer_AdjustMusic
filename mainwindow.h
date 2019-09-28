#pragma once

#include <QMainWindow>
#include <QMediaPlayer>
#include <lyricfile/lyricfile.h>
#include <lyricprocesslabel/lyricprocesslabel.h>
#include <QListView>
#include <QLabel>
#include <QVBoxLayout>
#include <QMutex>
#include <QThread>
#include <logtimewindow.h>
#include <QCloseEvent>

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   MainWindow(const QString& musicFile, const QString& lyricFile, QWidget *parent = nullptr);
   ~MainWindow();
   void closeEvent(QCloseEvent *event);

   cszt::LyricFile lyric;

private:
   void generateLyricLabels(int need);
   void updateLyricList();
   void updateUndoRedo();

   QString musicFile;
   QString lyricFile;
   QMediaPlayer mediaPlayer;
   QMutex lyricLock;
   bool progressSliderLock;
   QList<long> timeLogger;
   LogTimeWindow *logTimeWindow;
   QString tempAudio;

   class DeamonThread : public QThread {
public:
      explicit DeamonThread(MainWindow *window);
      virtual void run();
      void interrupt();

private:
      MainWindow *window;
      bool running;
   }
   *deamonThread;

   // 以下指针无需在析构函数中释放
   QAction *repeatAction;                                    // 重复播放
   QAction *playAction, *pauseAction;                        // 播放暂停
   QAction *undoAction, *redoAction;                         // 撤销重做
   QListView *lyricListView;                                 // 左侧歌词列表
   QLabel *currentTimeLabel;                                 // 当前时间标签
   QLabel *totalTimeLabel;                                   // 总时间标签
   QWidget *rightWidget;                                     // 右侧面板
   QSlider *progressSlider;                                  // 进度条
   QVBoxLayout *lyricLableContainer;                         // 次要歌词容器
   LyricProcessLabel *mainLyricLabel;                        // 主要歌词容器
   QList<LyricProcessLabel *> lyricLabels;                   // 主要歌词
   QAction *halfRate, *normalRate, *doubleRate, *customRate; // 各种速率
};
