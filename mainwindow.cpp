#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isImuOn(false)
    , isLedOn(false)
    , isCamOn(false)
    , isGimOn(false)
    , isRawMode(false)
{
    ui->setupUi(this);
    applyStyles();

    ui->portCombo->addItem("/dev/ttyACM0");
    ui->portCombo->addItem("/dev/ttyACM1");
    ui->baudCombo->addItem("9600");
    ui->baudCombo->addItem("115200");
    ui->baudCombo->setCurrentText("115200");

    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_connectButton_clicked() {
    QString portName = ui->portCombo->currentText().trimmed();
    int baud = ui->baudCombo->currentText().toInt();

    serial->setPortName(portName);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    qint64 pid = getSerialPortPID(portName);
    if (pid > 0) {
        ui->textBrowser->append("Port is busy by PID: " + QString::number(pid));
        return;
    }

    if (serial->isOpen()) {
        serial->close();
        ui->connectButton->setText("Disconnected");
        ui->connectButton->setProperty("connected", false);
        ui->connectButton->style()->unpolish(ui->connectButton);
        ui->connectButton->style()->polish(ui->connectButton);
        ui->textBrowser->append("Connection Closed.");
        
        isImuOn = isLedOn = isCamOn = isGimOn = isRawMode = false;
        ui->btnToggleImu->setText("IMU ON (100ms)");
        ui->btnToggleLed->setText("LED Toggle 1000");
        ui->btnToggleCam->setText("Camera ON");
        ui->btnToggleGim->setText("Gimbal ON");
        ui->btnToggleRaw->setText("Mode: MONITOR");
    } else if (serial->open(QIODevice::ReadWrite)) {
        ui->connectButton->setText("Connected");
        ui->connectButton->setProperty("connected", true);
        ui->connectButton->style()->unpolish(ui->connectButton);
        ui->connectButton->style()->polish(ui->connectButton);
        ui->textBrowser->append("Connection Opened.");
    }
}

qint64 MainWindow::getSerialPortPID(QString portPath) {
    QProcess process;
    process.start("lsof", QStringList() << "-t" << portPath);
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();
    return output.isEmpty() ? 0 : output.toLongLong();
}

void MainWindow::sendCommand(const QString &cmd) {
    if (serial->isOpen()) {
        QString fullCmd = cmd + "\r\n";
        serial->write(fullCmd.toUtf8());
        serial->flush();
        ui->textBrowser->append("TX -> " + cmd);
    }
}

// STM32에서 들어오는 모든 데이터를 읽고 처리하는 핵심 함수
void MainWindow::readData() {
    QByteArray data = serial->readAll();
    m_buffer.append(data);

    // 1. 수신된 모든 데이터를 텍스트 브라우저에 출력 (CLI 모드 확인용)
    ui->textBrowser->insertPlainText(QString::fromUtf8(data));
    ui->textBrowser->moveCursor(QTextCursor::End);

    // 2. 패킷 단위($...#) 파싱 처리
    while (m_buffer.contains('$') && m_buffer.contains('#')) {
        int start = m_buffer.indexOf('$');
        int end = m_buffer.indexOf('#', start);

        if (end == -1) break; // 아직 '#'이 다 안 들어왔으면 대기

        // '$'와 '#' 사이의 순수 데이터 추출 (예: "3,73:2:12.5,74:2:-3.2,75:2:90.0")
        QByteArray packet = m_buffer.mid(start + 1, end - start - 1);
        m_buffer.remove(0, end + 1);

        if (!packet.isEmpty()) {
            parseProtocol(packet);
        }
    }
}

// 추출된 패킷 데이터를 라벨에 업데이트
void MainWindow::parseProtocol(const QByteArray &packet) {
    QString data = QString::fromUtf8(packet);
    QStringList parts = data.split(','); // 쉼표(,)로 노드 분리

    if (parts.size() < 2) return; // 데이터가 없으면 무시

    // parts[0]은 데이터 갯수(count) 이므로 parts[1]부터 실제 센서 데이터
    for (int i = 1; i < parts.size(); ++i) {
        QStringList item = parts[i].split(':'); // 콜론(:)으로 ID, TYPE, VALUE 분리
        if (item.size() < 3) continue;

        int sensorID = item[0].toInt();
        QString valueStr = item[2]; // 수신된 센서 값

        switch (sensorID) {
            case ID_IMU_GYRO_X: ui->lblRoll->setText("Roll: " + valueStr + " °"); break;
            case ID_IMU_GYRO_Y: ui->lblPitch->setText("Pitch: " + valueStr + " °"); break;
            case ID_IMU_GYRO_Z: ui->lblYaw->setText("Yaw: " + valueStr + " °"); break;
            case ID_IMU_GIM:    ui->lblGimState->setText("Gimbal: " + valueStr + " °"); break;
            case ID_OUT_LED_STATE: ui->lblLed->setText("LED: " + QString(valueStr == "1" ? "ON" : "OFF")); break;
        }
    }
}

// -----------------------------------------------------
// 토글 버튼 이벤트 핸들러
// -----------------------------------------------------

void MainWindow::on_btnToggleImu_clicked() {
    isImuOn = !isImuOn;
    if (isImuOn) {
        sendCommand("imu on 100");
        ui->btnToggleImu->setText("IMU OFF");
    } else {
        sendCommand("imu off");
        ui->btnToggleImu->setText("IMU ON (100ms)");
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
    }
}

void MainWindow::on_btnToggleCam_clicked() {
    isCamOn = !isCamOn;
    if (isCamOn) {
        sendCommand("cam on"); 
        ui->btnToggleCam->setText("Camera OFF");
        ui->lblCamState->setText("Camera: Connected");
    } else {
        sendCommand("cam off");
        ui->btnToggleCam->setText("Camera ON");
        ui->lblCamState->setText("Camera: Disconnected");
    }
}

void MainWindow::on_btnToggleGim_clicked() {
    isGimOn = !isGimOn;
    if (isGimOn) {
        sendCommand("gim on 100");
        ui->btnToggleGim->setText("Gimbal OFF");
    } else {
        sendCommand("gim off");
        ui->btnToggleGim->setText("Gimbal ON");
    }
}

// 모니터/RAW 텍스트 모드 스위치 버튼
void MainWindow::on_btnToggleRaw_clicked() {
    isRawMode = !isRawMode;
    if (isRawMode) {
        // Raw 모드: 모니터 패킷(mon)을 끄고 순수 CLI 텍스트만 받음
        sendCommand("mon off");
        ui->btnToggleRaw->setText("Mode: RAW Text");
        ui->btnToggleRaw->setStyleSheet("background-color: #ffc107; color: black; font-weight: bold;");
    } else {
        // Monitor 모드: 패킷을 받아 라벨을 업데이트함
        sendCommand("mon on 100");
        ui->btnToggleRaw->setText("Mode: MONITOR");
        ui->btnToggleRaw->setStyleSheet("background-color: #17a2b8; color: black; font-weight: bold;");
    }
}

void MainWindow::applyStyles() {
    QString qss = R"(
        QPushButton#connectButton[connected="false"] {background-color: #555555; color: white; font-weight: bold;}
        QPushButton#connectButton[connected="true"]  {background-color: #28a745; color: white; font-weight: bold;}
        QPushButton { font-weight: bold; padding: 5px; }
        QLabel { font-size: 15px; font-weight: bold; color: #333333; background: #e9ecef; padding: 5px; border-radius: 5px;}
        QLabel#lblRoll, QLabel#lblPitch, QLabel#lblYaw { color: #0056b3; }
        QLabel#lblGimState { color: #d35400; }
    )";
    this->setStyleSheet(qss);
}