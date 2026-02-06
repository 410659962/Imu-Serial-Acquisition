#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QVBoxLayout>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QComboBox>
#include <QDebug>
#include <QTimer>
#include <QLabel>
#include <QDateTime>
#include <QtEndian>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QTextCodec>
#include <QScrollBar>
#include <QValueAxis>


QT_CHARTS_USE_NAMESPACE
namespace Ui {
class MainWindow;
}

struct IMUData {
    float accel[3];  // x, y, z (g)
    float gyro[3];   // x, y, z (deg/s)
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void scanSerialPorts();   // 扫描串口槽函数
    void onSerialDataReceived();
    void on_serial_port_switch_clicked();
    void updateDisplay();             // 更新显示（定时器触发）

    void on_savedata_clicked();
    void onAutoStopTimeout();         // 自动停止超时
    void on_clear_data_clicked();

private:
    Ui::MainWindow *ui;
    void initUI();
    void initCharts();                // 初始化图表
    // 检查串口是否仍然存在（用于处理USB拔出情况）
    bool checkPortAvailable(const QString &portName);
    QSerialPort *serialcheck;
    QTimer *scanTimer;
    bool isSerialOpen;

    // 数据解析
    int parseReceivedData();         // 解析接收缓冲区
    // 数据缓冲区
    QByteArray receiveBuffer;         // 原始接收缓冲区
    // 解析后的数据（9个IMU）
    IMUData imuData[9];
    bool dataValid;                   // 当前数据是否有效
    // 统计信息
    qint64 totalBytesReceived;        // 总接收字节数
    qint64 validFramesReceived;       // 有效帧数
    qint64 invalidFramesReceived;     // 无效帧数
    QDateTime lastFrameTime;          // 最后一帧时间
    float actualFrequency;            // 实际接收频率
    QDateTime displayLastUpdateTime;  // 上次显示更新时间
    int framesSinceLastDisplay;       // 上次显示以来收到的帧数

    QString pendingDisplayText;       // 缓冲待显示的文本
    int frameCounter = 0;             // 帧计数器，用于UI降频
    static const int UI_UPDATE_INTERVAL = 100; // 每100帧（1000ms）更新一次UI

    // 数据格式常量
    static const int IMU_COUNT = 9;           // IMU数量
    static const int DATA_PER_IMU = 6;        // 每个IMU的数据量（3轴accel + 3轴gyro）
    static const int FLOAT_SIZE = 4;          // float占4字节
    static const int HEAD_SIZE = 2;           // 帧头2字节
    static const int TAIL_SIZE = 4;           // 尾标4字节
    static const int DATA_SIZE = IMU_COUNT * DATA_PER_IMU * FLOAT_SIZE; // 216字节
    static const int FRAME_SIZE = HEAD_SIZE + DATA_SIZE + TAIL_SIZE;    // 222字节

    static const char HEAD_PATTERN[2];
    // 尾标定义：{0x00, 0x00, 0x80, 0x7f} 对应float的NaN或特定值
    static const char TAIL_PATTERN[4];

    QFile *saveFile;                  // 保存文件指针
    QTextStream *fileStream;          // 文件流
    bool isSaving;                    // 是否正在保存
    QTimer *autoStopTimer;            // 自动停止定时器
    void startSaving();               // 开始保存
    void stopSaving();                // 停止保存
    QString generateFileName();       // 生成文件名
    void saveDataToFile();            // 保存数据到文件
    int totalSaveSeconds;        // 用户设定的总保存时间（秒）
    int remainingSeconds;        // 剩余秒数
    QTimer *countdownTimer;      // 倒计时定时器（每秒更新）
    void updateCountdownDisplay();  // 更新显示

    // 图表相关
    QChart *chart;                    // 图表对象
    QChartView *chartView;            // 图表视图
    QLineSeries *accelSeries[3];      // 加速度曲线（X,Y,Z）
    QLineSeries *gyroSeries[3];       // 陀螺仪曲线（X,Y,Z）
    QDateTime startTime;              // 记录开始时间，用于计算相对时间
    static const int MAX_DISPLAY_SECONDS = 10;  // 最大显示10秒数据
    static const int DATA_INTERVAL_MS = 100;    // 数据间隔100ms（10Hz显示）
    void updateChart(float meanAccel[3], float meanGyro[3]);  // 更新图表
    static const int Chart_FPS = 10;  //刷新频率10Hz
    // 添加成员变量
    int chartUpdateCounter = 0;
    float lastMeanAccel[3] = {0};
    float lastMeanGyro[3] = {0};
    void clearCharts();            // 清除图表曲线

};

#endif // MAINWINDOW_H
