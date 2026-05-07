#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isImuOn(false)
    , isLedOn(false)
    , isGimOn(false)
    , isCamOn(false)
{
    ui->setupUi(this);

    applyStyles();
    ui->portCombo->addItem("/dev/ttyACM0"); // 리눅스 호환을 위해 /dev/ 붙임
    ui->portCombo->addItem("/dev/ttyACM1");
    ui->baudCombo->addItem("9600");
    ui->baudCombo->addItem("115200");
    ui->baudCombo->setCurrentText("115200"); // 기본 통신 속도

    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connectButton_clicked()
{
    QString portName = ui->portCombo->currentText().trimmed();
    int baud = ui->baudCombo->currentText().toInt();

    serial=new QSerialPort(this);
    serial->setPortName(portName);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    qint64 pid = getSerialPortPID(portName);

    if (pid > 0) {
        qDebug() << portName << "is already in use. PID:" << pid;
        ui->textBrowser->append("Port is busy by PID: " + QString::number(pid));
        return;
    }

    if (serial->isOpen()) {
        serial->close();
        ui->connectButton->setText("Disconnected");
        ui->connectButton->setProperty("connected", false);
        ui->connectButton->style()->unpolish(ui->connectButton);
        ui->connectButton->style()->polish(ui->connectButton);
        ui->textBrowser->append("Connection : Port Closed!!");

        // 연결 종료 시 상태 초기화
        isImuOn = isLedOn = isGimOn = isCamOn = false;
        ui->btnToggleImu->setText("IMU ON (500ms)");
        ui->btnToggleLed->setText("LED Toggle 1000");
        ui->btnToggleGim->setText("Temp 1000");
        ui->btnToggleCam->setText("Camera ON");
        return;
    }
    else if (serial->open(QIODevice::ReadWrite)) {
        ui->connectButton->setText("Connected");
        ui->connectButton->setProperty("connected", true);
        ui->connectButton->style()->unpolish(ui->connectButton);
        ui->connectButton->style()->polish(ui->connectButton);
        ui->textBrowser->append("Connection : Port Opened!!");
    }
    else {
        ui->connectButton->setText("Disconnected");
        ui->connectButton->setProperty("connected", false);
        ui->connectButton->style()->unpolish(ui->connectButton);
        ui->connectButton->style()->polish(ui->connectButton);
        ui->textBrowser->append("Connection : Port Open Error!!");
    }
}

qint64 MainWindow::getSerialPortPID(QString portPath) {
    QProcess process;
    process.start("lsof", QStringList() << "-t" << portPath);
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();
    if (output.isEmpty()) return 0;
    return output.toLongLong();
}

void MainWindow::sendCommand(const QString &cmd) {
    if (serial->isOpen()) {
        QString fullCmd = cmd + "\r\n";
        qint64 bytesWritten = serial->write(fullCmd.toUtf8());
        serial->flush();

        if (bytesWritten == -1) {
            ui->textBrowser->append("Transmit Error!!");
        } else {
            ui->textBrowser->append("TX -> " + cmd);
        }
    } else {
        ui->textBrowser->append("Error: Port not open.");
    }
}

void MainWindow::readData() {
    m_buffer.append(serial->readAll());

    while (m_buffer.contains('[') && m_buffer.contains(']')) {
        int start = m_buffer.indexOf('[');
        int end = m_buffer.indexOf(']', start);

        if (end == -1) break;

        QByteArray packet = m_buffer.mid(start + 1, end - start - 1);
        m_buffer.remove(0, end + 1);

        if (!packet.isEmpty()) {
            ui->textBrowser->append(packet); // 패킷 로그가 너무 많으면 주석 처리
            parseProtocol(packet);
        }
    }
}

void MainWindow::parseProtocol(const QByteArray &packet) {
    QString data = QString::fromUtf8(packet);
    QStringList parts = data.split(',');

    if (parts.size() < 2) return;

    for (int i = 1; i < parts.size(); ++i) {
        QStringList item = parts[i].split(':');
        if (item.size() < 3) continue;

        int sensorID = item[0].toInt();
        QString valueStr = item[2];

        // 센서 ID에 따른 라벨 업데이트
        if (sensorID == ID_IMU_GIM) {
            ui->lblGim->setText(QString("Gim: %1 °C").arg(valueStr));
        } else if (sensorID == ID_OUT_LED_STATE) {
            ui->lblLed->setText(QString("LED: %1").arg(valueStr == "1" ? "ON" : "OFF"));
        } else if (sensorID == ID_IMU_GYRO_X) {
            ui->lblRoll->setText(QString("Roll: %1 °").arg(valueStr));
        } else if (sensorID == ID_IMU_GYRO_Y) {
            ui->lblPitch->setText(QString("Pitch: %1 °").arg(valueStr));
        } else if (sensorID == ID_IMU_GYRO_Z) {
            ui->lblYaw->setText(QString("Yaw: %1 °").arg(valueStr));
        }
    }
}

// -----------------------------------------------------
// 토글 버튼 이벤트 핸들러
// -----------------------------------------------------

void MainWindow::on_btnToggleImu_clicked() {
    isImuOn = !isImuOn;
    if (isImuOn) {
        sendCommand("imu on 500");
        ui->btnToggleImu->setText("IMU OFF");
    } else {
        sendCommand("imu off");
        ui->btnToggleImu->setText("IMU ON (500ms)");
        ui->lblRoll->setText("Roll: --");
        ui->lblPitch->setText("Pitch: --");
        ui->lblYaw->setText("Yaw: --");
    }
}

void MainWindow::on_btnToggleLed_clicked() {
    isLedOn = !isLedOn;
    if (isLedOn) {
        sendCommand("led toggle 1000");
        ui->btnToggleLed->setText("LED OFF");
    } else {
        sendCommand("led off");
        ui->btnToggleLed->setText("LED Toggle 1000");
        ui->lblLed->setText("LED: OFF");
    }
}

void MainWindow::on_btnToggleGim_clicked() {
    isGimOn = !isGimOn;
    if (isGimOn) {
        sendCommand("gim on 500");
        ui->btnToggleGim->setText("Gim ON");
    } else {
        sendCommand("gim off"); // 기존 펌웨어에서 인자 없으면 StopAuto로 설계된 경우
        ui->btnToggleGim->setText("gim on 1000");
        ui->lblGim->setText("Gim: OFF");
    }
}

void MainWindow::on_btnToggleCam_clicked() {
    isCamOn = !isCamOn;
    if (isCamOn) {
        sendCommand("cam on"); // 펌웨어에 맞게 수정 필요
        ui->btnToggleCam->setText("Camera OFF");
        ui->lblCamState->setText("Camera: Connected");
    } else {
        sendCommand("cam off");
        ui->btnToggleCam->setText("Camera ON");
        ui->lblCamState->setText("Camera: Disconnected");
    }
}

void MainWindow::applyStyles() {
    QString qss = R"(
        QPushButton#connectButton[connected="false"] {background-color: #555555; color: white; font-weight: bold;}
        QPushButton#connectButton[connected="true"]  {background-color: #28a745; color: white; font-weight: bold;}
        QLabel { font-size: 14px; font-weight: bold; color: #333333; }
        QLabel#lblRoll, QLabel#lblPitch, QLabel#lblYaw { color: #0056b3; }
    )";
    this->setStyleSheet(qss);
}
