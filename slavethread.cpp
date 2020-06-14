#include "slavethread.h"

#include <QSerialPort>
#include <QTime>

SlaveThread::SlaveThread(QObject *parent)
    : QThread (parent) {}

SlaveThread::~SlaveThread()
{
    m_mutex.lock();
    m_quit = true;
    m_mutex.unlock();
    wait();
}

void SlaveThread::startSlave(const QString &portName, int waitTimeout, const QString &response)
{
    const QMutexLocker locker(&m_mutex);
    m_portName = portName;
    m_waitTimeout = waitTimeout;
    m_response = response;
    if (!isRunning())
        start();
}

    void SlaveThread::run()
    {
        bool currentPortNameChanged = false;
        m_mutex.lock();
        QString currentPortName;
        if (currentPortName != m_portName) {
            currentPortName = m_portName;
            currentPortNameChanged = true;
        }
        int currentWaitTimeout = m_waitTimeout;
        QString currentResponse = m_response;
        m_mutex.unlock();
        QSerialPort serial;
        while (!m_quit) {
            if (currentPortNameChanged) {
                serial.close();
                serial.setPortName(currentPortName);
                if (!serial.open(QIODevice::ReadWrite)) {
                    emit error(tr("Cant open %1, error code %2")
                               .arg(m_portName).arg(serial.error()));
                    return;
                }
            }
            if (serial.waitForReadyRead(currentWaitTimeout)) {
                QByteArray requestData = serial.readAll();
                while (serial.waitForReadyRead(10))
                    requestData += serial.readAll();
                const QByteArray responseData = currentResponse.toUtf8();
                serial.write(responseData);
                if (serial.waitForBytesWritten(m_waitTimeout)) {
                    const QString request = QString::fromUtf8(requestData);
                    emit this->request(request);
                } else {
                    emit timeout(tr("Wait write response timeout %1")
                                 .arg(QTime::currentTime().toString()));
                }
            } else {
                emit timeout(tr("Wait read request timeout %1")
                             .arg(QTime::currentTime().toString()));
            }
            m_mutex.lock();
            if (currentPortName != m_portName) {
                currentPortName = m_portName;
                currentPortNameChanged = true;
            } else {
                currentPortNameChanged = false;
            }
            currentWaitTimeout = m_waitTimeout;
            currentResponse = m_response;
            m_mutex.unlock();
        }
    }

