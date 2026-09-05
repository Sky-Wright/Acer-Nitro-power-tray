#include <QApplication>
#include <QCoreApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QAction>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QMessageBox>

// Path to the privileged helper installed by the package
static const QString HELPER_PATH = QStringLiteral("/usr/lib/linux-power-tray/set-governor");

class LinuxPowerTray : public QObject {
    Q_OBJECT

public:
    LinuxPowerTray() {
        ensureAutostartInstalled();

        trayIcon = new QSystemTrayIcon(this);
        menu = new QMenu();

        connect(menu, &QMenu::aboutToShow, this, &LinuxPowerTray::populateMenu);
        trayIcon->setContextMenu(menu);

        connect(trayIcon, &QSystemTrayIcon::activated,
                this, &LinuxPowerTray::onTrayActivated);

        // Poll every 2s:
        //  - Keep icon in sync if governor changes externally
        //  - Detect AC/battery state changes and re-apply current profile
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &LinuxPowerTray::checkStatus);
        timer->start(2000);

        // Restore last saved profile, or default to schedutil (Balanced)
        QString saved = loadSavedProfile();
        QString toApply = saved.isEmpty() ? QStringLiteral("schedutil") : saved;
        setGovernor(toApply, /*saveProfile=*/false);

        trayIcon->show();
    }

private slots:
    void populateMenu() {
        menu->clear();

        QString currentGov = getGovernor();

        QAction* perfAction = new QAction("Performance ⚡", this);
        perfAction->setCheckable(true);
        perfAction->setChecked(currentGov == "performance");
        connect(perfAction, &QAction::triggered, this, [this]() {
            setGovernor("performance");
        });
        menu->addAction(perfAction);

        QAction* balancedAction = new QAction("Balanced ⚖️", this);
        balancedAction->setCheckable(true);
        balancedAction->setChecked(
            currentGov == "schedutil" || currentGov == "ondemand");
        connect(balancedAction, &QAction::triggered, this, [this]() {
            setGovernor("schedutil");
        });
        menu->addAction(balancedAction);

        QAction* powerAction = new QAction("Powersave 🌙", this);
        powerAction->setCheckable(true);
        powerAction->setChecked(currentGov == "powersave");
        connect(powerAction, &QAction::triggered, this, [this]() {
            setGovernor("powersave");
        });
        menu->addAction(powerAction);

        menu->addSeparator();

        QAction* statusAction = new QAction(
            QString("Active: %1  |  %2")
                .arg(currentGov)
                .arg(isOnAC() ? "AC ⚡" : "Battery 🔋"),
            this);
        statusAction->setEnabled(false);
        menu->addAction(statusAction);

        menu->addSeparator();

        QAction* quitAction = new QAction("Exit", this);
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
        menu->addAction(quitAction);
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            togglePower();
        }
    }

    void checkStatus() {
        QString gov = getGovernor();
        updateIcon(gov);

        // If AC state changed since last poll, silently re-apply the saved
        // profile so ryzenadj uses the correct AC or battery TDP limits.
        bool ac = isOnAC();
        if (ac != lastAcState) {
            lastAcState = ac;
            QString saved = loadSavedProfile();
            if (!saved.isEmpty()) {
                setGovernor(saved, /*saveProfile=*/false);
            }
        }
    }

    void togglePower() {
        QString current = getGovernor();
        if (current == "powersave") {
            setGovernor("schedutil");
        } else if (current == "schedutil" || current == "ondemand") {
            setGovernor("performance");
        } else {
            setGovernor("powersave");
        }
    }

private:
    QSystemTrayIcon* trayIcon;
    QMenu* menu;
    bool lastAcState = false;

    // -----------------------------------------------------------------------
    // Profile persistence
    // -----------------------------------------------------------------------
    QString profileFilePath() {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                      + "/linux-power-tray";
        QDir().mkpath(dir);
        return dir + "/last-profile";
    }

    void saveProfile(const QString& gov) {
        QFile f(profileFilePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&f) << gov;
        }
    }

    QString loadSavedProfile() {
        QFile f(profileFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
        return QString(f.readLine()).trimmed();
    }

    // -----------------------------------------------------------------------
    // AC state
    // -----------------------------------------------------------------------
    bool isOnAC() {
        QFile f("/sys/class/power_supply/ACAD/online");
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return true;
        return QString(f.readLine()).trimmed() == "1";
    }

    // -----------------------------------------------------------------------
    // Governor helpers
    // -----------------------------------------------------------------------
    QString getGovernor() {
        QFile file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
        return QString(file.readLine()).trimmed();
    }

    void setGovernor(const QString& gov, bool saveProfile = true) {
        if (saveProfile) {
            this->saveProfile(gov);
        }

        QProcess proc;
        proc.start(HELPER_PATH, QStringList() << gov);

        // 10s timeout: ryzenadj via fork+exec may take a moment
        if (!proc.waitForFinished(10000)) {
            proc.kill();
        }

        QString actual = getGovernor();
        updateIcon(actual);
    }

    // -----------------------------------------------------------------------
    // Autostart
    // -----------------------------------------------------------------------
    void ensureAutostartInstalled() {
        QString autostartDir =
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + "/autostart";
        QDir().mkpath(autostartDir);

        QString filepath = autostartDir + "/linux-power-tray.desktop";
        if (QFile::exists(filepath)) return;

        QFile file(filepath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

        QTextStream out(&file);
        out << "[Desktop Entry]\n"
            << "Name=Linux Power Tray\n"
            << "Comment=Toggle between Performance, Balanced, and Powersave CPU governors\n"
            << "Exec=" << QCoreApplication::applicationFilePath() << "\n"
            << "Icon=preferences-system-power-management\n"
            << "Terminal=false\n"
            << "Type=Application\n"
            << "Categories=System;Utility;\n"
            << "X-KDE-StartupNotify=false\n";
    }

    // -----------------------------------------------------------------------
    // Icon
    // -----------------------------------------------------------------------
    void updateIcon(const QString& governor) {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        if (governor == "performance") {
            painter.setBrush(QBrush(QColor("#FF4500")));
            painter.setPen(Qt::NoPen);
            QPolygon bolt;
            bolt << QPoint(18, 2) << QPoint(6, 16)  << QPoint(15, 16)
                 << QPoint(12, 30) << QPoint(26, 14) << QPoint(17, 14);
            painter.drawPolygon(bolt);
            trayIcon->setToolTip("Linux Power: Performance ⚡");

        } else if (governor == "schedutil" || governor == "ondemand") {
            painter.setBrush(QBrush(QColor("#2ECC71")));
            painter.setPen(Qt::NoPen);
            QPainterPath leaf;
            leaf.moveTo(16, 4);
            leaf.quadTo(28, 12, 24, 24);
            leaf.quadTo(16, 28, 16, 28);
            leaf.quadTo(8,  24, 8,  24);
            leaf.quadTo(4,  12, 16, 4);
            painter.drawPath(leaf);
            painter.setPen(QPen(QColor("#FFFFFF"), 1.5));
            painter.drawLine(16, 26, 16, 10);
            trayIcon->setToolTip("Linux Power: Balanced ⚖️");

        } else if (governor == "powersave") {
            painter.setBrush(QBrush(QColor("#1E90FF")));
            painter.setPen(Qt::NoPen);
            QPainterPath full, cut, moon;
            full.addEllipse(4, 4, 24, 24);
            cut.addEllipse(10, 2, 24, 24);
            moon = full.subtracted(cut);
            painter.drawPath(moon);
            trayIcon->setToolTip("Linux Power: Powersave 🌙");

        } else {
            painter.setBrush(QBrush(QColor("#888888")));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(4, 4, 24, 24);
            painter.setPen(QPen(Qt::white, 2));
            painter.drawText(QRect(4, 4, 24, 24), Qt::AlignCenter, "?");
            trayIcon->setToolTip("Linux Power: Unknown state");
        }

        painter.end();
        trayIcon->setIcon(QIcon(pixmap));
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("linux-power-tray");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon::fromTheme("preferences-system-power-management"));

    LinuxPowerTray tray;
    return app.exec();
}

#include "main.moc"
