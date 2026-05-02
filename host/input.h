#pragma once
#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <QDebug>

class InputReader : public QObject {
  Q_OBJECT
public:
  enum class DeviceType { Stylus, Touch };

  explicit InputReader(const QString& device, DeviceType type,
                       QObject* parent = nullptr)
    : QObject(parent), m_device(device), m_type(type) {}

  void start() {
    m_fd = open(m_device.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) { qWarning() << "Cannot open" << m_device; return; }
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &InputReader::readEvents);
    qDebug() << "[input] Listening on" << m_device;
  }

  void stop() {
    if (m_notifier) m_notifier->setEnabled(false);
    if (m_fd >= 0) { close(m_fd); m_fd = -1; }
  }

signals:
  void pointerDown(double x, double y, double pressure);
  void pointerMove(double x, double y, double pressure);
  void pointerUp();

private slots:
  void readEvents() {
    struct input_event ev;
    while (read(m_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
      if (ev.type == EV_ABS) {
        switch (ev.code) {
          case ABS_X:                m_rawX = ev.value; break;
          case ABS_Y:                m_rawY = ev.value; break;
          case ABS_PRESSURE:         m_pressure = ev.value; break;
          case ABS_MT_POSITION_X:    m_rawX = ev.value; break;
          case ABS_MT_POSITION_Y:    m_rawY = ev.value; break;
          case ABS_MT_PRESSURE:      m_pressure = ev.value; break;
        }
      }
      if (ev.type == EV_KEY) {
        if (ev.code == BTN_TOUCH || ev.code == BTN_TOOL_PEN) {
          if (ev.value == 1) {
            m_down = true;
            emit pointerDown(normX(), normY(), normP());
          } else {
            m_down = false;
            emit pointerUp();
          }
        }
      }
      if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
        if (m_down) emit pointerMove(normX(), normY(), normP());
      }
    }
  }

private:
  double normX() const {
    double max = (m_type == DeviceType::Stylus) ? 11180.0 : 2064.0;
    return m_rawX / max;
  }
  double normY() const {
    double max = (m_type == DeviceType::Stylus) ? 15340.0 : 2832.0;
    return m_rawY / max;
  }
  double normP() const {
    double max = (m_type == DeviceType::Stylus) ? 4096.0 : 255.0;
    return m_pressure / max;
  }

  QString m_device;
  DeviceType m_type;
  QSocketNotifier* m_notifier = nullptr;
  int m_fd = -1;
  int m_rawX = 0, m_rawY = 0, m_pressure = 0;
  bool m_down = false;
};
