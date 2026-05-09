#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QProcess>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

typedef enum {
    /* 시스템 공통 상태 */
    ID_SYS_HEARTBEAT    = 0,
    ID_SYS_UPTIME       = 1,
    ID_SYS_VREF         = 3,

    /* 액추에이터 상태 피드백 */
    ID_OUT_LED_STATE    = 50,
    ID_OUT_MOTOR_SPEED  = 51,

    /* 모션 및 위치 데이터 */
    ID_IMU_ACCEL_X      = 70,
    ID_IMU_ACCEL_Y      = 71,
    ID_IMU_ACCEL_Z      = 72,
    ID_IMU_GYRO_X       = 73,   // Roll
    ID_IMU_GYRO_Y       = 74,   // Pitch
    ID_IMU_GYRO_Z       = 75,   // Yaw
    ID_IMU_ALL          = 76,   
    ID_IMU_GIM          = 77    // Gimbal Angle
} SensorID;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void readData();

    void on_btnToggleImu_clicked();
    void on_btnToggleLed_clicked();
    void on_btnToggleCam_clicked();
    void on_btnToggleGim_clicked();
    void on_btnToggleRaw_clicked(); // CLI Raw 텍스트 모드 전환 버튼

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    QByteArray m_buffer;

    // 상태 변수
    bool isImuOn;
    bool isLedOn;
    bool isCamOn;
    bool isGimOn;
    bool isRawMode; // true = Raw CLI 모드, false = 패킷 모니터 모드

    void parseProtocol(const QByteArray &packet);
    qint64 getSerialPortPID(QString portPath);
    void applyStyles();
    void sendCommand(const QString &cmd);
};
#endif // MAINWINDOW_H