#include "pch.h"
#include "mainwindow.h"

#include <QApplication>
#include <QSplitter>
#include <QGridLayout>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QCursor>
#include <QProcess>
#include <QStringListModel>
#include <QInputDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include "rawtextdialog.h"

#define BUTTON_PLAY_TEXT     "播放"
#define BUTTON_PAUSE_TEXT    "暂停"

/// 标记根据当前播放进度进行修改
/// 获取当前播放的进度、歌词信息
/// 并在不可修改时弹出对话框退出
#define MODIFY_CURRENT                                                        \
   long time = static_cast<int>(mediaPlayer.position());                      \
   cszt::Lyric lyric;                                                         \
   if (this->lyric.lyricByTime(lyric, time) == false)                         \
   {                                                                          \
      QApplication::beep();                                                   \
      QMessageBox::warning(this, "该歌词不可编辑", "该歌词根据文件名自动生成。如需编辑，请直接修改音乐文件名。"); \
      return;                                                                 \
   }

void MainWindow::generateLyricLabels(int need)
{
   int size = lyricLabels.size();

   for (int i = size; i < need; i++)
   {
      LyricProcessLabel *label = new LyricProcessLabel(rightWidget);
      QFont             font;
      font.setPixelSize(24);
      label->setFont(font);
      label->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
      label->setUnreachedColor(QColor(0, 0, 0));
      label->setTextAndProgress("", 0, 0, 0);
      connect(label, &LyricProcessLabel::clicked, [this, i] {
         MODIFY_CURRENT
         int size = lyric.texts.size();
         if (i >= size)
         {
            QApplication::beep();
            QMessageBox::warning(this, "该歌词不可编辑", "该歌词未在此处添加音轨，请先添加音轨后再操作。");
            return;
         }
         bool ok;
         QString input = QInputDialog::getText(this, "修改歌词", "输入歌词：", QLineEdit::Normal, lyric.texts[i], &ok);
         if (ok)
         {
            QMutexLocker lock(&lyricLock);
            this->lyric.setOtherLyricTextByTime(time, input, lyric.textLabels[i], i);
            updateUndoRedo();
         }
      });
      lyricLableContainer->addWidget(label);
      lyricLabels << label;
   }
}


inline void resetLyricLables(QList<LyricProcessLabel *>& labels)
{
   for (LyricProcessLabel *label:labels)
   {
      label->setTextAndProgress("", 0, 0, 0);
   }
}


MainWindow::MainWindow(const QString& musicFile, const QString& lyricFile, QWidget *parent) :
   QMainWindow(parent), musicFile(musicFile),
   lyricFile(lyricFile), progressSliderLock(false)
{
   setWindowTitle("调整歌词");
   resize(890, 640);
   // 菜单栏
   QMenuBar *menubar     = menuBar();
   QMenu    *fileMenu    = menubar->addMenu("文件");
   QMenu    *editMenu    = menubar->addMenu("编辑");
   QMenu    *controlMenu = menubar->addMenu("控制");
   QMenu    *toolMenu    = menubar->addMenu("工具");
   QMenu    *aboutMenu   = menubar->addMenu("关于");
   // 文件菜单
   QAction *openNew = fileMenu->addAction("调整其它音乐...");
   openNew->setShortcut(QKeySequence("ctrl+N"));
   connect(openNew, &QAction::triggered, [] {
      QProcess process;
      process.setProgram("AdjustMusic.exe");
      process.startDetached();
   });
   fileMenu->addSeparator();
   QAction *reload = fileMenu->addAction("重新加载歌词文件");
   reload->setShortcut(QKeySequence("ctrl+O"));
   connect(reload, &QAction::triggered, [this] {
      if (QMessageBox::question(this, "重新读取歌词", "该操作将放弃当前所有修改，且不可撤销。\n\n要重新读取歌词吗？") == QMessageBox::Yes)
      {
         QFile file(this->lyricFile);
         cszt::LyricFile newLyric;
         if (!(newLyric << file))
         {
            QApplication::beep();
            QMessageBox::critical(this, "重新读取歌词", "重新读取歌词失败");
            return;
         }
         QMutexLocker locker(&lyricLock);
         lyric = newLyric;
         lyric.enableHistory();
         generateLyricLabels(lyric.getMaxOtherSize());
         updateLyricList();
         updateUndoRedo();
      }
   });
   fileMenu->addSeparator();
   QAction *save = fileMenu->addAction("保存");
   save->setShortcut(QKeySequence("ctrl+S"));
   connect(save, &QAction::triggered, [this] {
      QFile file(this->lyricFile);
      if (!(lyric >> file))
      {
         QApplication::beep();
         QMessageBox::critical(this, "关闭", "保存失败");
      }
   });
   QAction *saveAs = fileMenu->addAction("另存为...");
   saveAs->setShortcut(QKeySequence("ctrl+A"));
   connect(saveAs, &QAction::triggered, [this] {
      QString savePath = QFileDialog::getSaveFileName(this, "选择歌词保存路径", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "增强歌词文件 (*.lrc)");
      if (savePath.isNull())
      {
         return;
      }
      QFile file(savePath);
      if (!(lyric >> file))
      {
         QApplication::beep();
         QMessageBox::critical(this, "保存失败", "无法保存歌词文件");
      }
   });
   fileMenu->addSeparator();
   QAction *exitAction = fileMenu->addAction("退出");
   exitAction->setShortcut(QKeySequence("alt+f4"));
   connect(exitAction, &QAction::triggered, [this] {
      close();
   });
   // 编辑菜单
   undoAction = editMenu->addAction("撤销");
   undoAction->setShortcut(QKeySequence("ctrl+Z"));
   undoAction->setEnabled(false);
   connect(undoAction, &QAction::triggered, [this] {
      QMutexLocker locker(&lyricLock);
      int index = lyric.undo();
      updateLyricList();
      updateUndoRedo();
      if (index > -1)
      {
         lyricListView->setCurrentIndex(dynamic_cast<QStringListModel *>(lyricListView->model())->index(index));
      }
   });
   redoAction = editMenu->addAction("重做");
   redoAction->setShortcut(QKeySequence("ctrl+Y"));
   redoAction->setEnabled(false);
   connect(redoAction, &QAction::triggered, [this] {
      QMutexLocker locker(&lyricLock);
      int index = lyric.redo();
      updateLyricList();
      updateUndoRedo();
      if (index > -1)
      {
         lyricListView->setCurrentIndex(dynamic_cast<QStringListModel *>(lyricListView->model())->index(index));
      }
   });
   editMenu->addSeparator();
   QAction *finishSentenceAction = editMenu->addAction("这句唱完了");
   finishSentenceAction->setShortcut(QKeySequence("ctrl+enter"));
   connect(finishSentenceAction, &QAction::triggered, [this] {
      QMutexLocker locker(&lyricLock);
      lyric.finishSentence(static_cast<long>(mediaPlayer.position()));
      updateUndoRedo();
   });
   connect(editMenu->addAction("在当前歌词处添加次要歌词音轨"), &QAction::triggered, [this] {
      MODIFY_CURRENT
      bool ok;
      QString text = QInputDialog::getText(this, "添加次要歌词音轨", "输入增加的次要歌词内容：", QLineEdit::Normal, QString(), &ok);
      if (ok)
      {
         QMutexLocker lock(&lyricLock);
         this->lyric.addOtherLyricByTime(time, text, QString());
         generateLyricLabels(this->lyric.getMaxOtherSize());
         updateUndoRedo();
      }
   });
   QAction *resetTime = editMenu->addAction("重置当前歌词时间");
   resetTime->setShortcut(QKeySequence("ctrl+delete"));
   connect(resetTime, &QAction::triggered, [this] {
      MODIFY_CURRENT
      QMutexLocker lock(&lyricLock);
      this->lyric.clearAllTimeByIndex(this->lyric.lyricIndexByTime(time));
      updateUndoRedo();
   });
   editMenu->addAction("在当前歌词添加关键点*...");
   editMenu->addSeparator();
   connect(editMenu->addAction("前面添加..."), &QAction::triggered, [this] {
      MODIFY_CURRENT
      int index = this->lyric.lyricIndexByTime(time);
      bool ok;
      QString text = QInputDialog::getText(this, "前面添加歌词", "请输入要添加的主要歌词内容：", QLineEdit::Normal, QString(), &ok);
      if (ok)
      {
         QMutexLocker lock(&lyricLock);
         this->lyric.addBefore(index, text);
         updateLyricList();
         updateUndoRedo();
      }
   });
   connect(editMenu->addAction("后面添加..."), &QAction::triggered, [this] {
      long time = static_cast<int>(mediaPlayer.position());
      int index = this->lyric.lyricIndexByTime(time);
      bool ok;
      QString text = QInputDialog::getText(this, "后面添加歌词", "请输入要添加的主要歌词内容：", QLineEdit::Normal, QString(), &ok);
      if (ok)
      {
         QMutexLocker lock(&lyricLock);
         this->lyric.addAfter(index, text);
         updateLyricList();
         updateUndoRedo();
      }
   });
   editMenu->addAction("打断歌词*...");
   connect(editMenu->addAction("克隆歌词"), &QAction::triggered, [this] {
      MODIFY_CURRENT
      QMutexLocker lock(&lyricLock);
      this->lyric.cloneSentence(this->lyric.lyricIndexByTime(time));
      updateLyricList();
      updateUndoRedo();
   });
   editMenu->addSeparator();
   QAction *resetAllTime = editMenu->addAction("清除全部歌词时间");
   resetAllTime->setShortcut(QKeySequence("ctrl+shift+delete"));
   connect(resetAllTime, &QAction::triggered, [this] {
      QMutexLocker lock(&lyricLock);
      this->lyric.clearAllTime();
      updateUndoRedo();
   });
   // 控制菜单
   playAction = controlMenu->addAction("播放");
   playAction->setShortcut(QKeySequence("space"));
   connect(playAction, &QAction::triggered, [this] {
      this->mediaPlayer.play();
   });
   QAction *replayAction = controlMenu->addAction("重新播放");
   replayAction->setShortcut(QKeySequence("F12"));
   connect(replayAction, &QAction::triggered, [this] {
      this->mediaPlayer.setPosition(0);
      this->mediaPlayer.play();
   });
   pauseAction = controlMenu->addAction("暂停");
   pauseAction->setShortcut(QKeySequence("space"));
   pauseAction->setEnabled(false);
   connect(pauseAction, &QAction::triggered, [this] {
      this->mediaPlayer.pause();
   });
   controlMenu->addSeparator();
   repeatAction = controlMenu->addAction("循环播放");
   repeatAction->setShortcut(QKeySequence("ctrl+R"));
   repeatAction->setCheckable(true);
   QMenu *controlSpeedMenu = controlMenu->addMenu("播放速度");
   halfRate   = controlSpeedMenu->addAction("半速");
   normalRate = controlSpeedMenu->addAction("正常");
   doubleRate = controlSpeedMenu->addAction("倍速");
   controlSpeedMenu->addSeparator();
   customRate = controlSpeedMenu->addAction("自定义...");
   halfRate->setShortcut(QKeySequence("ctrl+1"));
   normalRate->setShortcut(QKeySequence("ctrl+2"));
   doubleRate->setShortcut(QKeySequence("ctrl+3"));
   customRate->setShortcut(QKeySequence("ctrl+0"));
   halfRate->setCheckable(true);
   normalRate->setCheckable(true);
   doubleRate->setCheckable(true);
   customRate->setCheckable(true);
   normalRate->setChecked(true);
   connect(halfRate, &QAction::triggered, [this] {
      mediaPlayer.setPlaybackRate(0.5);
      halfRate->setChecked(true);
      normalRate->setChecked(false);
      doubleRate->setChecked(false);
      customRate->setChecked(false);
   });
   connect(normalRate, &QAction::triggered, [this] {
      mediaPlayer.setPlaybackRate(1);
      halfRate->setChecked(false);
      normalRate->setChecked(true);
      doubleRate->setChecked(false);
      customRate->setChecked(false);
   });
   connect(doubleRate, &QAction::triggered, [this] {
      mediaPlayer.setPlaybackRate(2);
      halfRate->setChecked(false);
      normalRate->setChecked(false);
      doubleRate->setChecked(true);
      customRate->setChecked(false);
   });
   connect(customRate, &QAction::triggered, [this] {
      bool ok;
      double rate = QInputDialog::getDouble(this, "自定义播放速度", "请输入播放速度，0~1为慢速播放，1~4为快速播放：", mediaPlayer.playbackRate(), 0.1, 4, 1, &ok);
      if (!ok)
      {
         rate = mediaPlayer.playbackRate();
      }
      mediaPlayer.setPlaybackRate(rate);
      normalRate->setChecked(false);
      halfRate->setChecked(false);
      doubleRate->setChecked(false);
      customRate->setChecked(false);
      if (abs(rate - 1) < 0.001)
      {
         normalRate->setChecked(true);
      }
      else if (abs(rate - 0.5) < 0.001)
      {
         halfRate->setChecked(true);
      }
      else if (abs(rate - 2) < 0.001)
      {
         doubleRate->setChecked(true);
      }
      else
      {
         customRate->setChecked(true);
      }
   });
   controlMenu->addSeparator();
   connect(controlMenu->addAction("修改音量..."), &QAction::triggered, [this] {
      bool ok;
      int volume = QInputDialog::getInt(this, "修改音量", "输入修改后的音量，范围0~100", mediaPlayer.volume(), 0, 100, 10, &ok);
      if (ok)
      {
         mediaPlayer.setVolume(volume);
      }
   });
   // 工具菜单
   QAction *logTimes = toolMenu->addAction("记录当前时间");
   logTimes->setShortcut(QKeySequence("ctrl++"));
   connect(logTimes, &QAction::triggered, [this] {
      long time = static_cast<long>(mediaPlayer.position());
      timeLogger << time;
      logTimeWindow->updateList(timeLogger);
   });
   QAction *showTimes = toolMenu->addAction("显示记录的时间");
   connect(showTimes, &QAction::triggered, [this] {
      logTimeWindow->show();
   });
   toolMenu->addSeparator();
   QAction *clearTimes = toolMenu->addAction("清除记录时间");
   connect(clearTimes, &QAction::triggered, [this] {
      timeLogger.clear();
      logTimeWindow->updateList(timeLogger);
   });
   // 关于菜单
   QAction *help = aboutMenu->addAction("帮助*");
   help->setShortcut(QKeySequence("F1"));
   aboutMenu->addSeparator();
   QAction *aboutQt = aboutMenu->addAction("关于 Qt...");
   aboutQt->setShortcut(QKeySequence("ctrl+F1"));
   connect(aboutQt, &QAction::triggered, [this] {
      QMessageBox::aboutQt(this, "关于 Qt");
   });
   // 主要界面
   // 左右分隔
   QSplitter *splitter = new QSplitter(this);
   splitter->setOrientation(Qt::Orientation::Horizontal);
   // 左侧歌词列表
   lyricListView = new QListView(splitter);
   lyricListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
   splitter->addWidget(lyricListView);
   connect(lyricListView, &QListView::clicked, [this](QModelIndex index) {
      long time = lyric.lyricTimeByIndex(index.row());
      mediaPlayer.setPosition(time + 1);
   });
   lyricListView->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(lyricListView, &QListView::customContextMenuRequested, [this] {
      QModelIndex currentIndex = lyricListView->currentIndex();
      if (currentIndex.isValid())
      {
         int index = currentIndex.row();
         cszt::Lyric lyric;
         this->lyric.lyricByIndex(lyric, index);
         int otherTextSize = lyric.texts.size();
         QMenu menu(lyricListView);
         menu.addAction(lyric.mainText)->setEnabled(false);
         connect(menu.addAction("前面添加..."), &QAction::triggered, [this, index] {
            bool ok;
            QString text = QInputDialog::getText(this, "前面添加歌词", "请输入要添加的主要歌词内容：", QLineEdit::Normal, QString(), &ok);
            if (ok)
            {
               QMutexLocker lock(&lyricLock);
               this->lyric.addBefore(index, text);
               updateLyricList();
               updateUndoRedo();
            }
         });
         connect(menu.addAction("后面添加..."), &QAction::triggered, [this, index] {
            bool ok;
            QString text = QInputDialog::getText(this, "后面添加歌词", "请输入要添加的主要歌词内容：", QLineEdit::Normal, QString(), &ok);
            if (ok)
            {
               QMutexLocker lock(&lyricLock);
               this->lyric.addAfter(index, text);
               updateLyricList();
               updateUndoRedo();
            }
         });
         menu.addAction("打断歌词*...");
         connect(menu.addAction("克隆歌词"), &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.cloneSentence(index);
            updateLyricList();
            updateUndoRedo();
         });
         menu.addSeparator();
         QMenu *modifyLyricMenu = menu.addMenu("修改歌词...");
         connect(modifyLyricMenu->addAction(lyric.mainText), &QAction::triggered, [this, lyric, index] {
            bool ok;
            QString input = QInputDialog::getText(this, "修改歌词", "输入歌词：", QLineEdit::Normal, lyric.mainText, &ok);
            if (ok)
            {
               QMutexLocker lock(&lyricLock);
               this->lyric.setMainLyricTextByIndex(index, input);
               updateLyricList();
               updateUndoRedo();
            }
         });
         for (int i = 0; i < otherTextSize; i++)
         {
            connect(modifyLyricMenu->addAction(lyric.texts[i]), &QAction::triggered, [this, lyric, index, i] {
               bool ok;
               QString input = QInputDialog::getText(this, "修改歌词", "输入歌词：", QLineEdit::Normal, lyric.texts[i], &ok);
               if (ok)
               {
                  QMutexLocker lock(&lyricLock);
                  this->lyric.setOtherLyricTextByIndex(index, input, lyric.textLabels[i], i);
                  updateUndoRedo();
               }
            });
         }
         connect(menu.addAction("增加次要歌词音轨..."), &QAction::triggered, [this, index] {
            bool ok;
            QString text = QInputDialog::getText(this, "增加次要歌词音轨", "输入增加的次要歌词内容：", QLineEdit::Normal, QString(), &ok);
            if (ok)
            {
               QMutexLocker lock(&lyricLock);
               this->lyric.addOtherLyricByIndex(index, text, QString());
               generateLyricLabels(this->lyric.getMaxOtherSize());
               updateUndoRedo();
            }
         });
         menu.addAction("增加关键点*...");
         menu.addSeparator();
         QAction *removeOtherLyricAction = menu.addAction("缩减轨道");
         if (lyric.texts.size() == 0)
         {
            removeOtherLyricAction->setEnabled(false);
         }
         connect(removeOtherLyricAction, &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.removeOtherLyricByIndex(index);
            updateUndoRedo();
         });
         menu.addAction("清除关键点*...");
         connect(menu.addAction("清除该句开始时间"), &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.clearStartTimeByIndex(index);
            updateUndoRedo();
         });
         QMenu *clearStatementAllKeyPointMenu = menu.addMenu("清除该句全部关键点");
         connect(clearStatementAllKeyPointMenu->addAction(lyric.mainText), &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.clearMainAllKeyPointByIndex(index);
            updateUndoRedo();
         });
         for (int i = 0; i < otherTextSize; i++)
         {
            connect(clearStatementAllKeyPointMenu->addAction(lyric.texts[i]), &QAction::triggered, [this, index, i] {
               QMutexLocker lock(&lyricLock);
               this->lyric.clearOtherAllKeyPointByIndex(index, i);
               updateUndoRedo();
            });
         }
         connect(menu.addAction("清除该句全部时间信息"), &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.clearAllTimeByIndex(index);
            updateUndoRedo();
         });
         connect(menu.addAction("删除该句歌词"), &QAction::triggered, [this, index] {
            QMutexLocker lock(&lyricLock);
            this->lyric.removeLyricAt(index);
            updateLyricList();
            updateUndoRedo();
         });
         menu.addSeparator();
         connect(menu.addAction("直接编辑原始内容"), &QAction::triggered, [this, lyric, index] {
            RawTextDialog dialog(this);
            dialog.setWindowTitle(lyric.mainText);
            QString rawText = this->lyric.printStatementByIndex(index);
            dialog.setString(rawText);
            if (dialog.exec() == QDialog::Accepted)
            {
               QString newText = dialog.string();
               if (newText != rawText)
               {
                  if (this->lyric.replaceStatement(index, newText))
                  {
                     QMutexLocker locker(&lyricLock);
                     generateLyricLabels(this->lyric.getMaxOtherSize());
                     updateLyricList();
                     updateUndoRedo();
                  }
                  else
                  {
                     QApplication::beep();
                     QMessageBox::critical(this, "编辑原始内容", "输入内容格式不正确！");
                  }
               }
            }
         });
         menu.exec(QCursor::pos());
      }
   });
   // 右侧面板
   rightWidget = new QWidget(splitter);
   splitter->addWidget(rightWidget);
   QGridLayout *rightWidgetLayout = new QGridLayout(rightWidget);
   // 上方时间、进度信息
   currentTimeLabel = new QLabel(rightWidget);
   totalTimeLabel   = new QLabel(rightWidget);
   progressSlider   = new QSlider(rightWidget);
   progressSlider->setOrientation(Qt::Orientation::Horizontal);
   rightWidgetLayout->addWidget(currentTimeLabel, 0, 0, Qt::AlignmentFlag::AlignLeft);
   rightWidgetLayout->addWidget(totalTimeLabel, 0, 2, Qt::AlignmentFlag::AlignRight);
   rightWidgetLayout->addWidget(progressSlider, 1, 0, 1, 3);
   connect(progressSlider, &QSlider::sliderPressed, [this] {
      progressSliderLock = true;
   });
   connect(progressSlider, &QSlider::sliderReleased, [this] {
      mediaPlayer.setPosition(progressSlider->value());
      progressSliderLock = false;
   });
   // 中央歌词用位置
   lyricLableContainer = new QVBoxLayout();
   mainLyricLabel      = new LyricProcessLabel(rightWidget);
   QFont font;
   font.setPixelSize(32);
   mainLyricLabel->setFont(font);
   mainLyricLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
   mainLyricLabel->setUnreachedColor(QColor(0, 0, 0));
   mainLyricLabel->setTextAndProgress("", 0, 0, 0);
   connect(mainLyricLabel, &LyricProcessLabel::clicked, [this] {
      MODIFY_CURRENT
      bool ok;
      QString input = QInputDialog::getText(this, "修改歌词", "输入歌词：", QLineEdit::Normal, lyric.mainText, &ok);
      if (ok)
      {
         QMutexLocker lock(&lyricLock);
         this->lyric.setMainLyricTextByTime(time, input);
         updateLyricList();
         updateUndoRedo();
      }
   });
   lyricLableContainer->addWidget(mainLyricLabel);
   rightWidgetLayout->addLayout(lyricLableContainer, 3, 0, 1, 3);
   // 上下均有分隔符
   rightWidgetLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding), 2, 1);
   rightWidgetLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding), 4, 1);
   // 下方控制器
   QWidget *controlWidget = new QWidget(rightWidget);
   rightWidgetLayout->addWidget(controlWidget, 5, 0, 1, 3, Qt::AlignmentFlag::AlignBottom);
   QGridLayout *controlWidgetLayout = new QGridLayout(controlWidget);
   QPushButton *playButton          = new QPushButton(controlWidget);
   playButton->setText(BUTTON_PLAY_TEXT);
   connect(playButton, &QPushButton::clicked, [this] {
      if (mediaPlayer.state() == QMediaPlayer::PlayingState)
      {
         mediaPlayer.pause();
      }
      else
      {
         mediaPlayer.play();
      }
   });
   QPushButton *finishStatement = new QPushButton(controlWidget);
   finishStatement->setText("这句唱完了");
   connect(finishStatement, &QPushButton::clicked, finishSentenceAction, &QAction::triggered);
   controlWidgetLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed), 0, 0);
   controlWidgetLayout->addWidget(playButton, 0, 1);
   controlWidgetLayout->addWidget(finishStatement, 0, 2);
   controlWidgetLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed), 0, 3);
   setCentralWidget(splitter);
   // 线程
   deamonThread = new DeamonThread(this);
   // 播放器事件
   connect(&mediaPlayer, &QMediaPlayer::durationChanged, [this](qint64 duration) {
      totalTimeLabel->setText(readableTimeString(duration));
      progressSlider->setRange(0, static_cast<int>(duration));
   });
   connect(&mediaPlayer, &QMediaPlayer::stateChanged, [this, playButton](QMediaPlayer::State state) {
      if (state == QMediaPlayer::PlayingState)
      {
         playButton->setText(BUTTON_PAUSE_TEXT);
         playAction->setEnabled(false);
         pauseAction->setEnabled(true);
      }
      else
      {
         playButton->setText(BUTTON_PLAY_TEXT);
         playAction->setEnabled(true);
         pauseAction->setEnabled(false);
      }
      if (state == QMediaPlayer::StoppedState)
      {
         if (repeatAction->isChecked())
         {
            mediaPlayer.setPosition(0);
            mediaPlayer.play();
         }
      }
   });
   connect(&mediaPlayer, &QMediaPlayer::positionChanged, [this] {
      mainLyricLabel->repaint();
      for (LyricProcessLabel *lyricLabel : lyricLabels)
      {
         lyricLabel->repaint();
      }
   });
   // 加载音乐
   mediaPlayer.setMedia(QUrl::fromLocalFile(musicFile));
   // 加载歌词
   if (lyricFile.toLower().endsWith(".txt"))
   {
      QFile lyricfile(lyricFile);
      if (!lyric.readAsTextFile(lyricfile))
      {
         QApplication::beep();
         QMessageBox::critical(this, "加载失败", "歌词文件读取失败：" + lyricFile);
         abort();
      }
   }
   else
   {
      QFile lyricfile(lyricFile);
      if (!(lyric << lyricfile))
      {
         QMessageBox::critical(this, "加载失败", "歌词文件读取失败：" + lyricFile);
         abort();
      }
   }
   lyric.enableHistory();
   generateLyricLabels(lyric.getMaxOtherSize());
   updateLyricList();
   deamonThread->start();
   logTimeWindow = new LogTimeWindow(this);
}


MainWindow::~MainWindow()
{
   deamonThread->interrupt();
   deamonThread->deleteLater();
   logTimeWindow->deleteLater();
}


void MainWindow::updateLyricList()
{
   QStringList list;

   lyric.allLyric(list);
   QAbstractItemModel *model = lyricListView->model();
   if (model)
   {
      model->deleteLater();
   }
   lyricListView->setModel(new QStringListModel(list));
}


void MainWindow::updateUndoRedo()
{
   undoAction->setEnabled(lyric.canUndo());
   undoAction->setText(QString("撤销 %1").arg(lyric.undoText()));
   redoAction->setEnabled(lyric.canRedo());
   redoAction->setText(QString("重做 %1").arg(lyric.redoText()));
}


void MainWindow::closeEvent(QCloseEvent *event)
{
   if (!lyric.canUndo())
   {
      event->accept();
      return;
   }
   int option = QMessageBox::question(this, "关闭", "关闭之前是否保存已修改内容？", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
   switch (option)
   {
   case QMessageBox::Yes:
      {
         QFile file(this->lyricFile);
         if (lyric >> file)
         {
            event->accept();
         }
         else
         {
            QApplication::beep();
            QMessageBox::critical(this, "关闭", "保存失败");
            event->ignore();
         }
         break;
      }

   case QMessageBox::No:
      event->accept();
      break;

   case QMessageBox::Cancel:
      event->ignore();
      break;
   }
}


MainWindow::DeamonThread::DeamonThread(MainWindow *window) : window(window), running(true)
{
}


void MainWindow::DeamonThread::run()
{
   QString name = window->musicFile.replace('\\', '/');

   if (name.endsWith("/"))
   {
      name = name.mid(0, name.length() - 1);
   }

   int index = name.lastIndexOf('/');
   if (index > 0)
   {
      name = name.mid(index + 1);
   }
   index = name.lastIndexOf('.');
   if (index > 0)
   {
      name = name.mid(0, index);
   }

   cszt::Lyric lyric;
   int         size;

   while (running)
   {
      msleep(50);
      {
         QMutexLocker lock(&window->lyricLock);
         long         position;
         if (window->progressSliderLock)
         {
            position = window->progressSlider->value();
         }
         else
         {
            position = static_cast<long>(window->mediaPlayer.position());
            window->progressSlider->setValue(static_cast<int>(position));
         }
         window->currentTimeLabel->setText(readableTimeString(position));
         if (window->lyric.lyricByTime(lyric, position))
         {
            if (!window->lyricListView->geometry().contains(window->mapFromGlobal(QCursor::pos())))
            {
               int lastIndex = window->lyricListView->currentIndex().row();
               int index     = window->lyric.lyricIndexByTime(position);
               if (lastIndex != index)
               {
                  window->lyricListView->setCurrentIndex(dynamic_cast<QStringListModel *>(window->lyricListView->model())->index(index));
               }
            }
            window->mainLyricLabel->setTextAndProgress(lyric.mainText, lyric.mainTextBreakStart, lyric.mainTextBreakEnd, lyric.mainTextBreakProgress);
            resetLyricLables(window->lyricLabels);
            size = lyric.texts.size();
            for (int i = 0; i < size; i++)
            {
               window->lyricLabels[i]->setTextAndProgress(lyric.texts[i], lyric.textBreakStarts[i], lyric.textBreakEnds[i], lyric.textBreakProgresses[i]);
            }
         }
         else
         {
            window->mainLyricLabel->setTextAndProgress(name, 0, name.length(), lyric.mainTextBreakProgress);
            resetLyricLables(window->lyricLabels);
         }
      }
   }
}


void MainWindow::DeamonThread::interrupt()
{
   running = false;
   wait();
}
