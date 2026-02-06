#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtEndian>
#include <QMessageBox>

// 帧头定义：{0xAA, 0x55}
const char MainWindow::HEAD_PATTERN[2] = {static_cast<char>(0xAA), static_cast<char>(0x55)};
// 尾标定义：{0x00, 0x00, 0x80, 0x7f} 对应float的NaN或特定值
const char MainWindow::TAIL_PATTERN[4] = {0x00, 0x00, static_cast<char>(0x80), 0x7f};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serialcheck = new QSerialPort(this);
    isSerialOpen = false;
    dataValid = false;
    totalBytesReceived = 0;
    validFramesReceived = 0;
    invalidFramesReceived = 0;
    actualFrequency = 0;
    totalSaveSeconds = 0;
    remainingSeconds = 0;
    saveFile = nullptr;
    fileStream = nullptr;
    isSaving = false;
    autoStopTimer = new QTimer(this);
    connect(autoStopTimer, &QTimer::timeout, this, &MainWindow::onAutoStopTimeout);

    // 倒计时定时器（每秒更新一次显示）
    countdownTimer = new QTimer(this);
    connect(countdownTimer, &QTimer::timeout, this, &MainWindow::updateCountdownDisplay);


    initUI();
    initCharts();

    // 初始化定时器，用于每2秒扫描一次
    scanTimer = new QTimer(this);
    connect(scanTimer, &QTimer::timeout, this, &MainWindow::scanSerialPorts);
    scanTimer->start(2000);
    // 信号槽连接
    connect(serialcheck, &QSerialPort::readyRead, this, &MainWindow::onSerialDataReceived);

    // 显示更新定时器（10fps，避免界面卡顿）
    QTimer *displayTimer = new QTimer(this);
    connect(displayTimer, &QTimer::timeout, this, &MainWindow::updateDisplay);
    displayTimer->start(100);
    // 立即执行一次扫描
    scanSerialPorts();

}

MainWindow::~MainWindow()
{
    delete ui;
    if (serialcheck->isOpen())  serialcheck->close();
    if (isSaving)   stopSaving();
}

void MainWindow::initUI()
{
    ui->serial_port_switch->setIcon(QIcon(":/img/close.png"));
    setWindowIcon(QIcon(":/img/3D_IMUArray.png"));

    // 波特率
    ui->serial_port_bund->addItem("9600", 9600);
    ui->serial_port_bund->addItem("115200", 115200);
    ui->serial_port_bund->addItem("460800", 460800);
    ui->serial_port_bund->setCurrentIndex(2); // 默认460800

    ui->serial_port_switch->setText("打开串口");
    ui->receiveTextEdit->clear();
    ui->receiveTextEdit_str->clear();
    ui->savedata->setText("开始保存");
    ui->savedata->setEnabled(false);  // 串口未打开时禁用保存按钮
    ui->checkBox_times->setChecked(false);
    ui->save_total_times->setText("0");
    ui->clear_data->setText("清除接收");


}

void MainWindow::initCharts()
{
    // 创建图表
    chart = new QChart();
    chart->setTitle("IMU Mean Data (Last 10 Seconds)");
    chart->setAnimationOptions(QChart::NoAnimation);  // 禁用动画提高性能

    // 创建6条曲线（3轴加速度 + 3轴陀螺仪）
    QString accelNames[3] = {"Accel X", "Accel Y", "Accel Z"};
    QString gyroNames[3] = {"Gyro X", "Gyro Y", "Gyro Z"};
    QColor accelColors[3] = {Qt::red, Qt::green, Qt::blue};
    QColor gyroColors[3] = {Qt::darkRed, Qt::darkGreen, Qt::darkBlue};

    // 初始化加速度曲线（实线）
    for (int i = 0; i < 3; i++) {
        accelSeries[i] = new QLineSeries();
        accelSeries[i]->setName(accelNames[i]);
        accelSeries[i]->setColor(accelColors[i]);
        accelSeries[i]->setPen(QPen(accelColors[i], 2, Qt::SolidLine));
        chart->addSeries(accelSeries[i]);
    }

    // 初始化陀螺仪曲线（虚线）
    for (int i = 0; i < 3; i++) {
        gyroSeries[i] = new QLineSeries();
        gyroSeries[i]->setName(gyroNames[i]);
        gyroSeries[i]->setColor(gyroColors[i]);
        gyroSeries[i]->setPen(QPen(gyroColors[i], 2, Qt::DashLine));
        chart->addSeries(gyroSeries[i]);
    }

    // 创建坐标轴
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Time (s)");
    axisX->setRange(0, MAX_DISPLAY_SECONDS);  // 显示0-10秒
    axisX->setTickCount(11);  // 每1秒一个刻度
    axisX->setLabelFormat("%.1f");
    chart->addAxis(axisX, Qt::AlignBottom);

    // 左Y轴：加速度 (g)
    QValueAxis *axisYAccel = new QValueAxis();
    axisYAccel->setTitleText("Accel (g)");
    axisYAccel->setRange(-2, 2);  // 加速度范围，可根据需要调整
    chart->addAxis(axisYAccel, Qt::AlignLeft);

    // 右Y轴：陀螺仪 (dps)
    QValueAxis *axisYGyro = new QValueAxis();
    axisYGyro->setTitleText("Gyro (dps)");
    axisYGyro->setRange(-250, 250);  // 陀螺仪范围，可根据需要调整
    chart->addAxis(axisYGyro, Qt::AlignRight);

    // 将曲线绑定到坐标轴
    for (int i = 0; i < 3; i++) {
        accelSeries[i]->attachAxis(axisX);
        accelSeries[i]->attachAxis(axisYAccel);
        gyroSeries[i]->attachAxis(axisX);
        gyroSeries[i]->attachAxis(axisYGyro);
    }

    // 创建图表视图并设置到graphicsView
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // 将chartView添加到graphicsView中
    QVBoxLayout *layout = new QVBoxLayout(ui->graphicsView);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView);

    // 记录开始时间
    startTime = QDateTime::currentDateTime();
}

void MainWindow::scanSerialPorts()
{
    // 获取当前所有可用端口
    QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
    // 保存当前用户选中的串口号
    QString currentSelection = ui->serial_port_com->currentText();
    // 只有当端口列表发生变化时才更新下拉框

    // 临时存储当前下拉框里的所有端口名
    QStringList existingPorts;
    for(int i=0; i<ui->serial_port_com->count(); ++i)
    {
        existingPorts << ui->serial_port_com->itemText(i);
    }
    QStringList newPorts;
        foreach (const QSerialPortInfo &info, portList) {
            newPorts << info.portName();
        }
    // 如果列表没变化，且当前选中的串口依然物理存在，则不做UI更新
    if (existingPorts == newPorts && !currentSelection.isEmpty() && checkPortAvailable(currentSelection))
    {
        return;
    }
    // 如果串口已经打开，且突然消失了（例如被拔掉），则自动关闭
    if (serialcheck->isOpen() && !checkPortAvailable(serialcheck->portName()))
    {
        on_serial_port_switch_clicked(); // 触发关闭逻辑
        qDebug() << "设备已拔出";
    }
    // 更新下拉框
    ui->serial_port_com->clear();
    foreach (const QSerialPortInfo &info, portList)
    {
        ui->serial_port_com->addItem(info.portName());
    }
    // 尝试恢复之前的选择
    int index = ui->serial_port_com->findText(currentSelection);
    if (index >= 0)
    {
        ui->serial_port_com->setCurrentIndex(index);
    }
}



bool MainWindow::checkPortAvailable(const QString &portName)
{
    QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
    foreach (const QSerialPortInfo &info, portList)
    {
        if (info.portName() == portName)    return true;
    }
    return false;
}

int MainWindow::parseReceivedData()
{
    // 数据不足，无法开始查找
    if (receiveBuffer.size() < FRAME_SIZE)
    {
        return 1; // 需要更多数据
    }

    // 滑动窗口查找帧头+尾标的完整帧
    int searchLimit = receiveBuffer.size() - HEAD_SIZE;

    for (int i = 0; i <= searchLimit; ++i)
    {
        // 检查帧头 (位置i)
        if (memcmp(receiveBuffer.constData() + i, HEAD_PATTERN, HEAD_SIZE) != 0)    continue;// 不是帧头，继续查找
        // 找到帧头，检查后面是否有足够的数据组成完整帧
        if (i + FRAME_SIZE > receiveBuffer.size())
        {
            // 数据不够，保留从帧头开始的数据，等待更多数据
            if (i > 0)  receiveBuffer.remove(0, i); // 移除帧头之前的垃圾数据
            return 1; // 需要更多数据
        }

        // 检查尾标 (位置 i + HEAD_SIZE + DATA_SIZE)
        int tailPos = i + HEAD_SIZE + DATA_SIZE;
        if (memcmp(receiveBuffer.constData() + tailPos, TAIL_PATTERN, TAIL_SIZE) != 0) continue;

        // 找到完整帧：帧头 + 数据 + 尾标 都匹配
        int dataStart = i + HEAD_SIZE; // 数据起始位置 = 帧头位置 + 2

        // 安全读取数据（避免内存对齐问题）
        float floatData[IMU_COUNT * DATA_PER_IMU];
        memcpy(floatData, receiveBuffer.constData() + dataStart, sizeof(floatData));
        // 计算9个IMU的均值
        float meanAccel[3] = {0.0f, 0.0f, 0.0f};
        float meanGyro[3] = {0.0f, 0.0f, 0.0f};

        for (int k = 0; k < IMU_COUNT; ++k)
        {
            imuData[k].accel[0] = floatData[k * 6 + 0];
            imuData[k].accel[1] = floatData[k * 6 + 1];
            imuData[k].accel[2] = floatData[k * 6 + 2];
            imuData[k].gyro[0]  = floatData[k * 6 + 3];
            imuData[k].gyro[1]  = floatData[k * 6 + 4];
            imuData[k].gyro[2]  = floatData[k * 6 + 5];

            meanAccel[0] += imuData[k].accel[0];
            meanAccel[1] += imuData[k].accel[1];
            meanAccel[2] += imuData[k].accel[2];
            meanGyro[0]  += imuData[k].gyro[0];
            meanGyro[1]  += imuData[k].gyro[1];
            meanGyro[2]  += imuData[k].gyro[2];
        }
        // 计算均值
        for (int j = 0; j < 3; ++j)
        {
            meanAccel[j] /= IMU_COUNT;
            meanGyro[j]  /= IMU_COUNT;
        }
        dataValid = true;

        // === 更新图表 ===
        memcpy(lastMeanAccel, meanAccel, sizeof(float)*3);
        memcpy(lastMeanGyro, meanGyro, sizeof(float)*3);
        chartUpdateCounter++;
        if (chartUpdateCounter >= Chart_FPS)
        {
            updateChart(lastMeanAccel, lastMeanGyro);  // 使用累积的数据
            chartUpdateCounter = 0;
        }
//        updateChart(meanAccel, meanGyro);

        // === 保存数据到文件 ===
        saveDataToFile();

        // 计算实际频率

        QDateTime now = QDateTime::currentDateTime();
        if (lastFrameTime.isValid())
        {
            double delta = lastFrameTime.msecsTo(now);
            if (delta > 0) actualFrequency = 1000.0 / delta;
        }
        lastFrameTime = now;
        framesSinceLastDisplay++;  // 计数帧数
        // === 显示数据到UI ===
        // 1. 构建当前帧的完整数据字符串（9个IMU的所有数据）

        // UI更新
        frameCounter++;

        QString frameData;
        for (int idx = 0; idx < IMU_COUNT; ++idx)
        {
            frameData += QString("IMU%1:%2,%3,%4,%5,%6,%7;")
                        .arg(idx + 1)
                        .arg(imuData[idx].accel[0], 0, 'f', 4)
                        .arg(imuData[idx].accel[1], 0, 'f', 4)
                        .arg(imuData[idx].accel[2], 0, 'f', 4)
                        .arg(imuData[idx].gyro[0], 0, 'f', 4)
                        .arg(imuData[idx].gyro[1], 0, 'f', 4)
                        .arg(imuData[idx].gyro[2], 0, 'f', 4);
        }
        frameData += "\r\n";  // 帧结束标记

        pendingDisplayText += frameData;

        if (frameCounter >= UI_UPDATE_INTERVAL || pendingDisplayText.size() > 1000)
        {
            if (ui->receiveTextEdit)
            {
                ui->receiveTextEdit->insertPlainText(pendingDisplayText);
                pendingDisplayText.clear();

                QTextCursor cursor = ui->receiveTextEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                ui->receiveTextEdit->setTextCursor(cursor);

                if (ui->receiveTextEdit->document()->lineCount() > 1000)
                {
                    QTextCursor cleanupCursor(ui->receiveTextEdit->document());
                    cleanupCursor.movePosition(QTextCursor::Start);
                    cleanupCursor.select(QTextCursor::BlockUnderCursor);
                    cleanupCursor.removeSelectedText();
                }
            }

            if (ui->receiveTextEdit_str)
            {
                QString meanData = QString("Mean:%1,%2,%3,%4,%5,%6")
                            .arg(meanAccel[0], 0, 'f', 4)
                            .arg(meanAccel[1], 0, 'f', 4)
                            .arg(meanAccel[2], 0, 'f', 4)
                            .arg(meanGyro[0], 0, 'f', 4)
                            .arg(meanGyro[1], 0, 'f', 4)
                            .arg(meanGyro[2], 0, 'f', 4);
                ui->receiveTextEdit_str->setPlainText(meanData);
            }

            frameCounter = 0;
        }
        // 移除完整帧（包括帧头、数据、尾标）
        receiveBuffer.remove(0, i + FRAME_SIZE);
        return 0;  // 成功解析
    }
    // 没找到任何完整帧，保留最后 (FRAME_SIZE-1) 字节，其余丢弃
    if (receiveBuffer.size() > FRAME_SIZE - 1)
    {
        int keep = FRAME_SIZE - 1;
        QByteArray temp = receiveBuffer.right(keep);
        receiveBuffer = temp;
    }
    return 1; // 需要更多数据
}

void MainWindow::startSaving()
{
    QString fileName = generateFileName();
    saveFile = new QFile(fileName);
    if (!saveFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误",
                   QString("无法创建文件: %1").arg(saveFile->errorString()));
        delete saveFile;
        saveFile = nullptr;
        return;
    }

    fileStream = new QTextStream(saveFile);
    // 设置编码为UTF-8
    fileStream->setCodec(QTextCodec::codecForName("UTF-8"));
    isSaving = true;
    ui->savedata->setText("停止保存");

    // 检查是否需要自动停止
    if (ui->checkBox_times->isChecked())
    {
        bool ok;
        int seconds = ui->save_total_times->toPlainText().toInt(&ok);
        if (ok && seconds > 0)
        {
            totalSaveSeconds = seconds;      // 保存设定值
            remainingSeconds = seconds;      // 初始化剩余时间
            autoStopTimer->start(seconds * 1000); // 转换为毫秒
            countdownTimer->start(1000);            // 每秒更新倒计时
            // 立即更新显示为倒计时格式
            ui->save_total_times->setText(QString("%1").arg(remainingSeconds));
            qDebug() << "将在" << seconds << "秒后自动停止保存";
        }
    }
    qDebug() << "开始保存数据到:" << fileName;
}

void MainWindow::stopSaving()
{
    // 停止倒计时
    if (countdownTimer->isActive()) countdownTimer->stop();
    // 停止自动停止定时器
    if (autoStopTimer->isActive())  autoStopTimer->stop();
    // 恢复显示为设定值（如果有设定的话）
    if (totalSaveSeconds > 0)
    {
        ui->save_total_times->setText(QString::number(totalSaveSeconds));
        totalSaveSeconds = 0;  // 重置
    }

    if (fileStream)
    {
        fileStream->flush();
        delete fileStream;
        fileStream = nullptr;
    }
    if(saveFile)
    {
        saveFile->close();
        delete saveFile;
        saveFile = nullptr;
    }
    isSaving = false;
    ui->savedata->setText("开始保存");
    qDebug() << "停止保存数据";
}

QString MainWindow::generateFileName()
{
    // 获取桌面路径
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    // 生成文件名：年月日_时分.csv（精确到分钟）
    QString dateTimeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmm");
    QString fileName = QString("%1/IMU_Data_%2.csv").arg(desktopPath).arg(dateTimeStr);
    // 如果文件已存在，添加秒数区分
    if (QFile::exists(fileName))
    {
        dateTimeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        fileName = QString("%1/IMU_Data_%2.csv").arg(desktopPath).arg(dateTimeStr);
    }
    return fileName;
}

void MainWindow::saveDataToFile()
{
    if (!isSaving || !fileStream || !dataValid) return;
    // 获取当前时间戳（精确到毫秒）
    qint64 timestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

    // 构建CSV行
    QString line = QString::number(timestamp);
    // 构建CSV行：时间戳 + 9个IMU的数据（每个IMU 6个值）
    for (int i = 0; i < IMU_COUNT; ++i)
    {
        // 添加accel和gyro数据，用逗号分隔
        line += QString(",%1,%2,%3,%4,%5,%6")
                .arg(imuData[i].accel[0], 0, 'f', 6)  // 提高精度到6位小数
                .arg(imuData[i].accel[1], 0, 'f', 6)
                .arg(imuData[i].accel[2], 0, 'f', 6)
                .arg(imuData[i].gyro[0], 0, 'f', 6)
                .arg(imuData[i].gyro[1], 0, 'f', 6)
                .arg(imuData[i].gyro[2], 0, 'f', 6);
    }
    *fileStream << line << "\n";
    // 每100帧刷新一次，提高性能
    if (validFramesReceived % 100 == 0) fileStream->flush();
}

void MainWindow::updateCountdownDisplay()
{
    remainingSeconds--;

    if (remainingSeconds > 0)
    {
        // 显示剩余时间
        ui->save_total_times->setText(QString("%1").arg(remainingSeconds));
    }
    else
    {
        // 倒计时结束，停止定时器
        countdownTimer->stop();
        // 显示会由 stopSaving 恢复
    }
}

void MainWindow::updateChart(float meanAccel[3], float meanGyro[3])
{
    // 计算相对时间（秒）
    qreal currentTime = startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;

    // 添加数据点到各条曲线
    for (int i = 0; i < 3; i++) {
        accelSeries[i]->append(currentTime, meanAccel[i]);
        gyroSeries[i]->append(currentTime, meanGyro[i]);
    }

    // 移除超过10秒的旧数据
    qreal minTime = currentTime - MAX_DISPLAY_SECONDS;
    for (int i = 0; i < 3; i++) {
        // 获取并清理加速度数据
        QVector<QPointF> accelPoints = accelSeries[i]->pointsVector();
        int removeCount = 0;
        for (const QPointF &point : accelPoints) {
            if (point.x() < minTime) {
                removeCount++;
            } else {
                break;  // 数据是按时间顺序的，找到一个不用的后面的都不用
            }
        }
        for (int j = 0; j < removeCount; j++) {
            accelSeries[i]->remove(0);
        }

        // 获取并清理陀螺仪数据
        QVector<QPointF> gyroPoints = gyroSeries[i]->pointsVector();
        removeCount = 0;
        for (const QPointF &point : gyroPoints) {
            if (point.x() < minTime) {
                removeCount++;
            } else {
                break;
            }
        }
        for (int j = 0; j < removeCount; j++) {
            gyroSeries[i]->remove(0);
        }
    }

    // 更新X轴范围，实现滚动效果
    if (currentTime > MAX_DISPLAY_SECONDS) {
        chart->axisX()->setRange(currentTime - MAX_DISPLAY_SECONDS, currentTime);
    } else {
        chart->axisX()->setRange(0, MAX_DISPLAY_SECONDS);
    }
}

void MainWindow::clearCharts()
{
    // 清除6条曲线的所有数据点
    for (int i = 0; i < 3; i++) {
        accelSeries[i]->clear();   // 清除加速度曲线
        gyroSeries[i]->clear();    // 清除陀螺仪曲线
    }

    // 重置X轴范围到初始状态（0-10秒）
    chart->axisX()->setRange(0, MAX_DISPLAY_SECONDS);

    // 重置开始时间，让新的数据从0秒开始
    startTime = QDateTime::currentDateTime();

    qDebug() << "图表已清除";
}

void MainWindow::on_serial_port_switch_clicked()
{
    if (isSerialOpen)
    {
        // 关闭串口
        if (serialcheck->isOpen())
        {
            serialcheck->close();
        }
        isSerialOpen = false;

        dataValid = false;

        ui->serial_port_switch->setText("打开串口");
        ui->serial_port_switch->setIcon(QIcon(":/img/close.png"));
        ui->serial_port_com->setEnabled(true);
        ui->serial_port_bund->setEnabled(true);

        // 串口关闭时，如果正在保存则停止保存，并禁用保存按钮
        if (isSaving)   stopSaving();
        ui->savedata->setEnabled(false);  // 禁用保存按钮

        qDebug() << "串口已关闭";
    }
    else
    {
        // 打开串口
        serialcheck->setPortName(ui->serial_port_com->currentText());
        // 设置波特率
        qint32 baudRate = ui->serial_port_bund->currentData().toInt();
        serialcheck->setBaudRate(baudRate);

        // --- 以下是程序内部固定设置的参数 ---
        serialcheck->setDataBits(QSerialPort::Data8);      // 数据位：8
        serialcheck->setStopBits(QSerialPort::OneStop);    // 停止位：1
        serialcheck->setParity(QSerialPort::NoParity);     // 校验位：无
        serialcheck->setFlowControl(QSerialPort::NoFlowControl); // 流控制：无
        // ------------------------------------
        if (serialcheck->open(QIODevice::ReadWrite))
        {
            isSerialOpen = true;
            ui->serial_port_switch->setText("关闭串口");
            ui->serial_port_switch->setIcon(QIcon(":/img/open.png"));
            ui->serial_port_com->setEnabled(false);
            ui->serial_port_bund->setEnabled(false);

            ui->savedata->setEnabled(true);  // 串口打开后启用保存按钮

            qDebug() << "已连接";
        }
        else
        {
            QMessageBox::critical(this, "错误",
                            QString("无法打开串口: %1").arg(serialcheck->errorString()));
        }
    }
}



void MainWindow::onSerialDataReceived()
{
    QByteArray newData = serialcheck->readAll();
    totalBytesReceived += newData.size();

    // 添加到缓冲区
    receiveBuffer.append(newData);

    // 循环解析，可能包含多帧
    int parseCount = 0;
    const int MAX_PARSE_PER_CALL = 10; // 防止单次处理过多

    while (receiveBuffer.size() >= FRAME_SIZE && parseCount < MAX_PARSE_PER_CALL)
    {
        int result = parseReceivedData();

        if (result == 0)  // 成功解析一帧
        {
            validFramesReceived++;
            parseCount++;
            // 继续循环，检查是否还有下一帧
        }
        else if (result == 1)  // 数据不足，停止解析等待更多数据
        {
            break;
        }

        // 防止缓冲区无限增长（超过3帧数据仍未找到合法帧，清空）
        if (receiveBuffer.size() > FRAME_SIZE * 3)
        {
            qDebug() << "缓冲区溢出，丢弃" << receiveBuffer.size() << "字节";
            receiveBuffer.clear();
            invalidFramesReceived++;
            break;
        }
    }
}



void MainWindow::updateDisplay()
{
    if (!dataValid) return;

    QString displayText;

    displayText += QString("=== 接收统计 ===\n");
    displayText += QString("总字节数: %1  有效帧: %2  无效帧: %3\n")
            .arg(totalBytesReceived).arg(validFramesReceived).arg(invalidFramesReceived);
    // 确保 actualFrequency 有有效值
    if (actualFrequency <= 0 || qIsNaN(actualFrequency))
    {
        displayText += QString("实际频率: 计算中... (理论100Hz)\n\n");
    }
    else
    {
        displayText += QString("实际频率: %1 Hz (理论100Hz)\n\n")
                        .arg(actualFrequency, 0, 'f', 1);  // 明确指定格式
    }

    for (int i = 0; i < IMU_COUNT; ++i)
    {
        displayText += QString("【IMU %1】\n").arg(i + 1);
        displayText += QString("  Accel(g):  X=%1  Y=%2  Z=%3\n")
                    .arg(imuData[i].accel[0], 8, 'f', 4)
                    .arg(imuData[i].accel[1], 8, 'f', 4)
                    .arg(imuData[i].accel[2], 8, 'f', 4);
        displayText += QString("  Gyro(dps): X=%1  Y=%2  Z=%3\n")
                    .arg(imuData[i].gyro[0], 8, 'f', 4)
                    .arg(imuData[i].gyro[1], 8, 'f', 4)
                    .arg(imuData[i].gyro[2], 8, 'f', 4);
        displayText += "\n";
    }
    if (ui->receiveTextEdit)
    {
        ui->textEdit_display->setPlainText(displayText);
    }

}

void MainWindow::on_savedata_clicked()
{
    if (isSaving) stopSaving();
    else startSaving();
}

void MainWindow::onAutoStopTimeout()
{
    if (isSaving)
    {
        stopSaving();
        qDebug() << "到达设定时间，自动停止保存";
    }
}

void MainWindow::on_clear_data_clicked()
{
    ui->textEdit_display->clear();
    ui->receiveTextEdit->clear();
    ui->receiveTextEdit_str->clear();
    clearCharts();
}
