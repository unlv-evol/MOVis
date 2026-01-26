#include "headers/mainwindow.h"
#include "headers/similarityInterface.h"
#include "headers/helpers/githubHelpers.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSettings>
#include <QDebug>
#include <QCloseEvent>
#include <QDir>
#include <QPixmap>
#include <QFileInfo>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QStandardPaths>
#include <QFileDialog>
#include <QString>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("MOVis");
    ui->btnExtra->hide();

    // Optional: placeholder text
    ui->lineText->setPlaceholderText("Enter a Repo Name...");
    ui->lineText_2->setPlaceholderText("Enter a Repo Name...");
    ui->lineText_3->setPlaceholderText("Enter a Project Name...");

    ui->lineText->setText("apache/kafka");
    ui->lineText_2->setText("linkedin/kafka");

    // Hide all other areas until we need them live.
    ui->dateBox->setVisible(false);
    ui->btnSubmit->hide();
    if(!hasSubdirectories())
        ui->oldResults->hide();

    // Date/time control formatting
    ui->dateInput->setDisplayFormat("yyyy-MM-dd");
    ui->dateInput->setCalendarPopup(true);   // nice calendar for date picking
    ui->dateInput->setDateTime(QDateTime::currentDateTime());

    // Date/time control formatting
    ui->dateEdit->setDisplayFormat("yyyy-MM-dd");
    ui->dateEdit->setCalendarPopup(true);   // nice calendar for date picking
    ui->dateEdit->setDateTime(QDateTime::currentDateTime());

    // Load saved data (tokens etc.)
    loadPersisted();

    // Force the label to have a minimum size and expand
    ui->imageLabel->setMinimumSize(400, 300);
    ui->imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->imageLabel->setAlignment(Qt::AlignCenter);

    // Connect button
    connect(ui->btnSubmit, &QPushButton::clicked, this, &MainWindow::onSubmit);

    // Connect button
    connect(ui->btnExtra, &QPushButton::clicked, this, &MainWindow::onAdvanced);

    // Connect button
    connect(ui->oldResults, &QPushButton::clicked, this, &MainWindow::onResults);

    // connect button
    connect(ui->divDateButton, &QPushButton::clicked, this, &MainWindow::onDivergenceCheck);

    // connect button
    connect(ui->hideButton, &QPushButton::clicked, this, [this](){
        ui->plainTextEdit->setVisible(!ui->plainTextEdit->isVisible());
        if(ui->plainTextEdit->isVisible()){
            ui->hideButton->setText("Hide");
        }else{
            ui->hideButton->setText("Show");
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::hasSubdirectories()
{
    QDir dir(QDir::currentPath() + "/Results/Repos_results");
    if (!dir.exists())
        return false;

    // Only list subdirectories, skip "." and ".."
    const QFileInfoList entries =
        dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    return !entries.isEmpty();
}

void MainWindow::showMessage(const QString &message, unsigned int id) {
    static QMessageBox* msgBox = nullptr;

    if (!msgBox) {
        msgBox = new QMessageBox;
        msgBox->setWindowTitle("Current Run Information");
        msgBox->setAttribute(Qt::WA_DeleteOnClose, false); // Do not delete when closed
    }

    msgBox->setText(message);

    // Set buttons based on id
    if (id == 151) {
        msgBox->setStandardButtons(QMessageBox::Ok);
    } else {
        msgBox->setStandardButtons(QMessageBox::NoButton); // No buttons
    }

    // Bring to front
    msgBox->raise();
    msgBox->activateWindow();
    QApplication::beep();

    if (!msgBox->isVisible()) {
        msgBox->show(); // Non-blocking
    }

    msgBox->update();
    QApplication::processEvents();
}

void MainWindow::onDivergenceCheck(){
    if(ui->lineText->text().isEmpty() || ui->lineText_2->text().isEmpty()){
        showMessage("Source or Target Variant are empty. Try again.", 151);
        return;
    }

    QStringList tokenList =
        ui->plainTextEdit->toPlainText()
            .split(QRegularExpression("[\r\n]+"),
                   Qt::SkipEmptyParts);

    if(tokenList.isEmpty()){
        showMessage("You need at least one token. Try again.", 151);
        return;
    }

    DivergenceResult r = divergence_date_qt(ui->lineText->text().trimmed(),
                       ui->lineText_2->text().trimmed(),
                       tokenList, 0);

    QDateTime divergence = QDateTime::fromString(r.divergeDate, Qt::ISODate);
    divergence.toTimeZone(QTimeZone::UTC);
    ui->dateEdit->setDate(divergence.date());

    QDateTime latest = QDateTime::fromString(r.leastDate, Qt::ISODate);
    latest.toTimeZone(QTimeZone::UTC);
    ui->dateInput->setDate(latest.date());

    ui->dateBox->setVisible(true);
    ui->btnSubmit->show();
}

void MainWindow::onAdvanced() {
    const QString origRepo = ui->lineText->text().trimmed();
    const QString divRepo  = ui->lineText_2->text().trimmed();
    const QDateTime from   = ui->dateEdit->dateTime();
    const QString fromIso  = from.toString(Qt::ISODate);
    const QDateTime when   = ui->dateInput->dateTime();
    const QString whenIso  = when.toString(Qt::ISODate);           // 2025-09-16T13:46:00
    const QString tokens   = ui->plainTextEdit->toPlainText();
    const QString projName = ui->lineText_3->text().trimmed();

    QString  results =  QDir::currentPath() + "/Results/Repos_results/"+projName;
    qDebug()<<results;
    similarityInterface *f = new similarityInterface(nullptr, results);
    f->setAttribute(Qt::WA_DeleteOnClose); // cleans up when user closes
    f->setWindowFlag(Qt::Window, true);    // ensures it's a top-level window
    f->show();
    f->raise();
    f->activateWindow();

}

static QString detectPython(const QString &venvDir = QString()) {
#ifdef Q_OS_WIN
    if (!venvDir.isEmpty()) {
        QString cand = venvDir + "/Scripts/python.exe";
        if (QFileInfo::exists(cand)) return cand;
    }
    // Works if the Python Launcher is installed
    if (QStandardPaths::findExecutable("py").size()) return "py";
    return "python"; // fallback (PATH)
#else
    if (!venvDir.isEmpty()) {
        QString cand = venvDir + "/bin/python3";
        if (QFileInfo::exists(cand)) return cand;
    }
    if (QStandardPaths::findExecutable("python3").size()) return "python3";
    return "python";
#endif
}

bool runPythonOnce(const QString &script, const QStringList &args) {
    QProcess p;
    p.start(detectPython(), QStringList() << script << args);
    if (!p.waitForStarted(5000)) return false;
    p.waitForFinished(-1);

    QString output = p.readAllStandardOutput();
    QString errorOutput = p.readAllStandardError();

    // Pretty printing function
    auto printSection = [](const QString &title, const QString &content, const QString &colorCode) {
        if (!content.trimmed().isEmpty()) {
            // Use ANSI colors if terminal supports them
            QString header = QString("\033[1m%1\033[0m").arg(title); // bold title
            QString colored = QString("%1%2\033[0m").arg(colorCode, content.trimmed());
            qDebug().noquote() << header << "\n" << colored << "\n";
        }
    };

    // Print errors in red, normal output in green
    printSection("Python stderr:", errorOutput, "\033[31m"); // red
    printSection("Python stdout:", output, "\033[32m");     // green


    return true;
}

void MainWindow::onResults(){
    QString startDir = "Results/Repos_results";

    if (!QDir(startDir).exists()) {
        startDir = QDir::homePath();  // fallback
    }

    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Root Directory",
        startDir
        );

    similarityInterface *f = new similarityInterface(nullptr, dir);
    f->setAttribute(Qt::WA_DeleteOnClose); // cleans up when user closes
    f->setWindowFlag(Qt::Window, true);    // ensures it's a top-level window
    f->show();
    f->raise();
    f->activateWindow();

}

void MainWindow::onSubmit() {
    const QString origRepo = ui->lineText->text().trimmed();
    const QString divRepo  = ui->lineText_2->text().trimmed();
    const QDateTime from   = ui->dateEdit->dateTime();
    const QString fromIso  = from.toString(Qt::ISODate);
    const QDateTime when   = ui->dateInput->dateTime();
    const QString whenIso  = when.toString(Qt::ISODate);           // 2025-09-16T13:46:00
    const QString tokens   = ui->plainTextEdit->toPlainText();
    const QString projName = ui->lineText_3->text().trimmed();

    if(projName.isEmpty() || projName.length()<1){
        showMessage("Empty Results Folder Name - Please provide a name.", 151);
        return;
    }

    const QString projectResults =
        QDir::currentPath() + "/Results/Repos_results/" + projName;

    if (QDir(projectResults).exists()) {
        showMessage("Results folders must be unique - please give us a new one.", 151);
        return;
    }

    if(origRepo.isEmpty() || divRepo.isEmpty()){
        showMessage("Source or Target Variant are empty. Try again.", 151);
        return;
    }

    if(tokens.size() == 0){
        showMessage("You need at least 1 token to run the program.", 151);
        return;
    }

    if(fromIso.isEmpty() || whenIso.isEmpty()){
        showMessage("Divergence dates are empty.", 151);
        return;
    }

    if(when < from){
        showMessage("Divergence Date is less than Initial Date.", 151);
        return;
    }

    // ---- Print to terminal (Qt Creator -> Application Output) ----
    qDebug().noquote() << "Submit clicked:";
    qDebug().noquote() << "  Original Repo:" << origRepo;
    qDebug().noquote() << "  Divergent Repo:" << divRepo;
    qDebug().noquote() << "  Initial DateTime ISO:"<< fromIso;
    qDebug().noquote() << "  DateTime ISO:" << whenIso;
    qDebug().noquote() << "  Project Name:" << projName;
    qDebug().noquote() << "  Tokens (lines):";
    for (const QString &line : tokens.split('\n', Qt::SkipEmptyParts)) {
        qDebug().noquote() << "   - " << line;
    }

    // Save tokens immediately as well (optional, we also save on close)
    savePersisted();
    showMessage("Currently Running Pareco - Please Wait a few minutes.", 0);
    QString script = QDir::currentPath()+"/Pareco/Pareco.py";
    QStringList args = {
        "-p", origRepo,
        "-d", divRepo,
        "-sd", fromIso.split("T")[0]+"T00:00:00Z",
        "-ed", whenIso.split("T")[0]+"T23:59:59Z",
        "-n", projName
    };

    QString newOrigRepo = origRepo.split("/").join("_");
    QString newDivRepo = divRepo.split("/").join("_");

    try {
        if(runPythonOnce(script, args)){
            const QString path = QDir::currentPath() + "/"+projName+"-"+newOrigRepo+"-"+newDivRepo;

            QPixmap pix(path);
            if (pix.isNull()) {
                showMessage("Could not load " + path, 151);
                return;
            }
            ui->imageLabel->setPixmap(pix.scaled(ui->imageLabel->size(),
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));

            ui->btnExtra->show();
            showMessage("Pareco has finished running - Click \"Advanced Resutls\" for more information.", 151);
        }
    } catch (...) {
        showMessage("Failed to open results. Look at logs for more information.", 151);
    }
    ui->oldResults->show();
}

void MainWindow::loadPersisted() {
    QSettings s;  // uses org/app from main.cpp

    // Restore token list text (entire plain text)
    const QString tokens = s.value("tokens/plainText", "").toString();
    ui->plainTextEdit->setPlainText(tokens);

    // (Optional) restore window geometry
    restoreGeometry(s.value("ui/geometry").toByteArray());
}

void MainWindow::savePersisted() const {
    QSettings s;

    // Persist token list
    s.setValue("tokens/plainText", ui->plainTextEdit->toPlainText());

    // (Optional) persist geometry
    s.setValue("ui/geometry", saveGeometry());

    // Also persist to file: Pareco/tokens.txt
    QString baseDir = QDir::currentPath();
    QString filePath = baseDir + "/tokens.txt";

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << ui->plainTextEdit->toPlainText();
        file.close();
    } else {
        QMessageBox::warning(nullptr, "Save Error",
                             "Could not save tokens to:\n" + filePath);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    savePersisted();
    QMainWindow::closeEvent(event);
}
