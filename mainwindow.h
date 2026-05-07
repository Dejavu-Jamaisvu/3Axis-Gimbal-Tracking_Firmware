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
    /* 0~9: 시스템 공통 상태 */
    ID_SYS_HEARTBEAT    = 0,
    ID_SYS_UPTIME       = 1,
    ID_SYS_TEMP         = 2,
    ID_SYS_VREF         = 3,

    /* 10~29: 환경 센서 데이터 */
    ID_ENV_TEMP         = 10,
    ID_ENV_HUMI         = 11,
    ID_ENV_PRESS        = 12,
    ID_ENV_LIGHT        = 13,

    /* 30~49: 사용자 입력 기기 */
    ID_IN_BUTTON_1      = 30,
    ID_IN_BUTTON_2      = 31,
    ID_IN_SW_DIP        = 32,
    ID_IN_ENC_POS       = 33,

    /* 50~69: 액추에이터 상태 피드백 */
    ID_OUT_LED_STATE    = 50,
    ID_OUT_MOTOR_SPEED  = 51,
    ID_OUT_RELAY        = 52,

    /* 70~89: 모션 및 위치 데이터 */
    ID_IMU_ACCEL_X      = 70,
    ID_IMU_ACCEL_Y      = 71,
    ID_IMU_ACCEL_Z      = 72,
    ID_IMU_GYRO_X       = 73, // Roll
    ID_IMU_GYRO_Y       = 74, // Pitch
    ID_IMU_GYRO_Z       = 75, // Yaw
    ID_IMU_GIM          = 76,  // GIMBAL

    /* 100+: 에러 및 알람 코드 */
    ID_ALARM_CRITICAL   = 100,
    ID_ALARM_WARN       = 101
} SensorID;

typedef enum {
    TYPE_UINT8   = 0,
    TYPE_INT32   = 1,
    TYPE_FLOAT   = 2,
    TYPE_BOOL    = 3,
    TYPE_STRING  = 4
} DataType;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void readData();

    // 새로운 토글 버튼 슬롯
    void on_btnToggleImu_clicked();
    void on_btnToggleLed_clicked();
    void on_btnToggleGim_clicked();
    void on_btnToggleCam_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    QByteArray m_buffer;

    // 상태 변수
    bool isImuOn;
    bool isLedOn;
    bool isGimOn;
    bool isCamOn;

    void parseProtocol(const QByteArray &packet);
    qint64 getSerialPortPID(QString portPath);
    void applyStyles();
    void sendCommand(const QString &cmd); // 중복 코드 제거용 헬퍼 함수
};
#endif // MAINWINDOW_H
