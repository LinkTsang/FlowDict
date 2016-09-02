﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFile>
#include <QTime>
#include <QMessageBox>
#include <QString>
#include <QCloseEvent>
#include <QRegExp>
#include <QWebEngineView>
#include <QWebChannel>
#include <QMenu>
#include <QKeySequence>
#include <QStringListModel>

#include "CCompleter.h"
#include "dialogabout.h"
#include "dictmanager.h"
#include "dictionary.h"
#include "ConfigParser.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),

      caseSensitiveLabel_(new QLabel(QStringLiteral("区分大小写"))),
      correctLabel_(new QLabel(QStringLiteral("智能拼写纠正"))),
      reverseLookupLabel_(new QLabel(QStringLiteral("反向查找"))),
      wildcardLabel_(new QLabel(QStringLiteral("使用通配符"))),

      caseSensitiveAct_(new QAction(QStringLiteral("区分大小写"))),
      changeSearchModeAct_(new QAction(QStringLiteral("切换查找模式"))),
      correctAct_(new QAction(QStringLiteral("智能拼写纠正"))),
      reverseLookupAct_(new QAction(QStringLiteral("反向查找"))),
      wildcardAct_(new QAction(QStringLiteral("使用通配符"))),

      completer_(new CCompleter),
      wordCompleterList_(new QStringListModel),

      dictWebPage_(new DictWebPage),
      webChannel_(new QWebChannel),

      findInPageMenu_(new QMenu),
      searchMenu_(new QMenu),
      systemTrayIconMenu_(new QMenu),
      systemTrayIcon_(new QSystemTrayIcon) {
  ui->setupUi(this);

  connect(ui->webEngineView, &QWebEngineView::loadFinished, this,
          &MainWindow::on_webEngineView_loadFinished);
  connect(dictWebPage_, &DictWebPage::linkClicked, this,
          &MainWindow::on_dictWebPage_linkClicked);
  dictWebPage_->setUrl(QUrl("qrc:/html/page.htm"));
  dictWebPage_->setWebChannel(webChannel_);

  // Completer
  completer_->setModel(wordCompleterList_);
  completer_->setTarget(ui->comboBox);
  // Search
  {
    clearStatusBarLabels();
    ui->statusBar->addPermanentWidget(caseSensitiveLabel_);
    ui->statusBar->addPermanentWidget(correctLabel_);
    ui->statusBar->addPermanentWidget(reverseLookupLabel_);
    ui->statusBar->addPermanentWidget(wildcardLabel_);

    changeSearchModeAct_->setShortcut(QKeySequence::Find);
    connect(changeSearchModeAct_, &QAction::triggered, this, [this]() {
      clearStatusBarLabels();
      if (ui->toolButton->menu() == searchMenu_) {
        ui->label->setText(QStringLiteral("解释内查找"));
        ui->toolButton->setMenu(findInPageMenu_);
        reverseLookupLabel_->setVisible(reverseLookupAct_->isChecked());
        caseSensitiveLabel_->setVisible(caseSensitiveAct_->isChecked());
        completer_->setModel(wordCompleterList_);
      } else if (ui->toolButton->menu() == findInPageMenu_) {
        ui->label->setText(QStringLiteral("查找"));
        ui->toolButton->setMenu(searchMenu_);
        correctLabel_->setVisible(correctAct_->isChecked());
        wildcardLabel_->setVisible(wildcardAct_->isChecked());
        completer_->setModel(nullptr);
      }
      ui->comboBox->setFocus();
    });

    wildcardAct_->setCheckable(true);
    wildcardAct_->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_W));
    connect(wildcardAct_, &QAction::toggled, wildcardLabel_,
            &QLabel::setVisible);
    correctAct_->setCheckable(true);
    correctAct_->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
    connect(correctAct_, &QAction::toggled, correctLabel_, &QLabel::setVisible);
    searchMenu_->addAction(wildcardAct_);
    searchMenu_->addAction(correctAct_);
    searchMenu_->addAction(changeSearchModeAct_);
    ui->toolButton->setMenu(searchMenu_);

    caseSensitiveAct_->setCheckable(true);
    caseSensitiveAct_->setShortcut(
        QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C));
    connect(caseSensitiveAct_, &QAction::toggled, caseSensitiveLabel_,
            &QLabel::setVisible);
    reverseLookupAct_->setCheckable(true);
    reverseLookupAct_->setShortcut(
        QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_R));
    connect(reverseLookupAct_, &QAction::toggled, reverseLookupLabel_,
            &QLabel::setVisible);
    findInPageMenu_->addAction(caseSensitiveAct_);
    findInPageMenu_->addAction(reverseLookupAct_);
    findInPageMenu_->addAction(changeSearchModeAct_);
  }

  // TrayIcon
  {
    systemTrayIconMenu_->addMenu(ui->menu);
    systemTrayIconMenu_->addMenu(ui->menu_2);
    systemTrayIconMenu_->addAction(QStringLiteral("退出(&X)"), this,
                                   &MainWindow::close);
    systemTrayIcon_->setContextMenu(systemTrayIconMenu_);
    connect(systemTrayIcon_, &QSystemTrayIcon::activated, this,
            &MainWindow::on_systemTrayIcon_activated);
    systemTrayIcon_->setToolTip(this->windowTitle());
    systemTrayIcon_->setIcon(QIcon(":/images/dict.png"));
  }

  Dictionary::Load(this);
}

MainWindow::~MainWindow() {
  delete ui;

  delete caseSensitiveAct_;
  delete changeSearchModeAct_;
  delete correctAct_;
  delete reverseLookupAct_;
  delete wildcardAct_;

  delete caseSensitiveLabel_;
  delete correctLabel_;
  delete reverseLookupLabel_;
  delete wildcardLabel_;

  delete completer_;
  delete wordCompleterList_;

  delete searchMenu_;
  delete findInPageMenu_;
  delete dictWebPage_;
  delete webChannel_;
  delete systemTrayIcon_;
  delete systemTrayIconMenu_;
}

void MainWindow::on_action_A_triggered() {
  DialogAbout* dialog = new DialogAbout;
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->exec();
}

void MainWindow::on_action_M_triggered() {
  DictManager* dialog = new DictManager;
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->exec();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  Config& config = Config::Get();
  if (!config.dump("config.json"))
    QMessageBox::warning(this, QStringLiteral("警告"),
                         "保存配置文件 config.json 失败!");
  event->accept();
}

void MainWindow::clearStatusBarLabels() const {
  correctLabel_->hide();
  wildcardLabel_->hide();
  caseSensitiveLabel_->hide();
  reverseLookupLabel_->hide();
}

void MainWindow::findInPage(const QString& text) const {
  QWebEnginePage::FindFlags flags;
  if (caseSensitiveAct_->isChecked())
    flags.setFlag(QWebEnginePage::FindCaseSensitively);
  if (reverseLookupAct_->isChecked())
    flags.setFlag(QWebEnginePage::FindBackward);
  ui->webEngineView->findText(text, flags);
}

void MainWindow::on_dictWebPage_linkClicked(const QUrl& url) {
  QString word = url.fileName();
  if (url.scheme() == "lookup") {
    ui->comboBox->setEditText(word);
    emit ui->comboBox->activated(word);
  }
}

void MainWindow::on_webEngineView_loadFinished(bool) {
  if (ui->webEngineView->url().url() == QStringLiteral("qrc:/html/cover.htm")) {
    this->show();
    systemTrayIcon_->show();
#ifdef Q_OS_WIN
    ::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);
#endif
  }
}

void MainWindow::changeEvent(QEvent* e) {
  if ((e->type() == QEvent::WindowStateChange) && this->isMinimized()) {
    this->hide();
#ifdef Q_OS_WIN
    ::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);
#endif
  }
  QMainWindow::changeEvent(e);
}

void MainWindow::search(const QString& text) const {
  if (ui->toolButton->menu() == searchMenu_) {
    ui->webEngineView->setPage(dictWebPage_);
    dictWebPage_->runJavaScript(QStringLiteral("clearArticles();"));
    statusBar()->showMessage(QStringLiteral("查询中..."));
    QTime time;
    time.start();

    QRegExp rx(R"(\\([\s\S])|(["\r\n]))");
    QString article;
    for (const auto& dict : Dictionary::Get()) {
      article.clear();
      dict->getArticleText(text, article);
      if (article.isEmpty()) continue;
      article.replace(rx, "\\\\1\\2");
      dictWebPage_->runJavaScript(QStringLiteral("addArticle(\"%1\",\"%2\");")
                                      .arg(dict->info().title, article));
    }
    QString info = QString("Time used:%1ms").arg(time.elapsed());
    statusBar()->showMessage(info);
  } else if (ui->toolButton->menu() == findInPageMenu_) {
    findInPage(text);
  }
}

void MainWindow::on_systemTrayIcon_activated(
    QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::Trigger:
      if (isMinimized()) {
        showNormal();
      } else if (!isVisible()) {
        show();
        qApp->setActiveWindow(this);
      }
      activateWindow();
      raise();
      break;
    default:
      break;
  }
}

void MainWindow::on_comboBox_activated(const QString& text) {
  completer_->completed();
  search(text);
}

void MainWindow::on_comboBox_editTextChanged(const QString& text) {
  if (ui->toolButton->menu() == searchMenu_) {
    const auto& dictionaries = Dictionary::Get();
    if (dictionaries.isEmpty()) return;

    QStringList keys;
    if (wildcardAct_->isChecked()) {
      keys = dictionaries[0]->keysThatMatch(text, 15);
    } else {
      keys = dictionaries[0]->keysWithPrefix(text, 15);
    }
    wordCompleterList_->setStringList(keys);
    completer_->complete();
  } else if (ui->toolButton->menu() == findInPageMenu_) {
    findInPage(text);
  }
}

void MainWindow::on_toolButton_clicked() {
  search(ui->comboBox->currentText());
}
