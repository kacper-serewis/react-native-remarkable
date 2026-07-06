#pragma once
// Raw evdev input for quill takeover mode. With xochitl stopped there is no
// windowing system, so the host reads /dev/input/event* directly — the same
// approach riddle uses (pen.rs / touch.rs), ported to the Qt event loop via
// QSocketNotifier.
#include <QSocketNotifier>
#include <QString>
#include <QDebug>
#include <algorithm>
#include <fstream>
#include <functional>
#include <string>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Find /dev/input/eventN whose device name contains one of the needles
// (case-insensitive). Returns an empty string if none matches.
inline QString findInputDevice(std::initializer_list<const char*> needles) {
  for (int i = 0; i < 32; i++) {
    std::ifstream f("/sys/class/input/event" + std::to_string(i) + "/device/name");
    if (!f) continue;
    std::string name;
    std::getline(f, name);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (const char* n : needles)
      if (name.find(n) != std::string::npos)
        return QString::fromStdString("/dev/input/event" + std::to_string(i));
  }
  return {};
}

// ── Base reader: open + grab + drain via the Qt event loop ───────
class EvdevReader {
public:
  virtual ~EvdevReader() { stop(); }

  bool start(const QString& device) {
    m_fd = open(device.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
    if (m_fd < 0) { qWarning() << "[input] cannot open" << device; return false; }
    // Grab so nothing else (e.g. a respawned vendor process) consumes events.
    ioctl(m_fd, EVIOCGRAB, 1);
    opened();
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read);
    QObject::connect(m_notifier, &QSocketNotifier::activated,
                     [this] { drain(); });
    qDebug() << "[input] listening on" << device;
    return true;
  }

  void stop() {
    delete m_notifier;
    m_notifier = nullptr;
    if (m_fd >= 0) {
      ioctl(m_fd, EVIOCGRAB, 0);
      close(m_fd);
      m_fd = -1;
    }
  }

protected:
  virtual void opened() {}
  virtual void handle(const input_event& ev) = 0;
  int m_fd = -1;

private:
  void drain() {
    input_event ev;
    while (read(m_fd, &ev, sizeof ev) == (ssize_t)sizeof ev) handle(ev);
  }
  QSocketNotifier* m_notifier = nullptr;
};

// ── Pointer: touchscreen (MT protocol B, first contact) or pen ───
// Coordinates are emitted normalized to 0..1; the caller scales to screen
// pixels and applies any axis flips.
class PointerReader : public EvdevReader {
public:
  std::function<void(double, double)> onDown;
  std::function<void(double, double)> onMove;
  std::function<void()> onUp;
  std::function<void()> onFiveFinger;  // takeover escape gesture

protected:
  void opened() override {
    // Prefer multitouch axes (touchscreen); fall back to plain ABS (pen).
    m_mt = absRange(ABS_MT_POSITION_X, m_minX, m_maxX) &&
           absRange(ABS_MT_POSITION_Y, m_minY, m_maxY);
    if (!m_mt) {
      absRange(ABS_X, m_minX, m_maxX);
      absRange(ABS_Y, m_minY, m_maxY);
    }
    qDebug() << "[input] axes x" << m_minX << ".." << m_maxX
             << "y" << m_minY << ".." << m_maxY << (m_mt ? "(mt)" : "(pen)");
  }

  void handle(const input_event& ev) override {
    if (ev.type == EV_ABS) {
      switch (ev.code) {
        case ABS_MT_SLOT:
          m_slot = std::clamp((int)ev.value, 0, kSlots - 1);
          break;
        case ABS_MT_TRACKING_ID: {
          m_active[m_slot] = (ev.value != -1);
          if (m_slot == 0) { m_down = m_active[0]; m_edge = true; }
          int fingers = 0;
          for (bool a : m_active) fingers += a;
          if (fingers >= 5 && onFiveFinger) onFiveFinger();
          break;
        }
        case ABS_MT_POSITION_X: if (m_slot == 0) { m_x = ev.value; m_moved = true; } break;
        case ABS_MT_POSITION_Y: if (m_slot == 0) { m_y = ev.value; m_moved = true; } break;
        case ABS_X: if (!m_mt) { m_x = ev.value; m_moved = true; } break;
        case ABS_Y: if (!m_mt) { m_y = ev.value; m_moved = true; } break;
      }
    } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH && !m_mt) {
      // Pen tip contact (hover moves arrive without BTN_TOUCH).
      m_down = (ev.value != 0);
      m_edge = true;
    } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
      if (m_edge) {
        m_edge = false;
        if (m_down) { if (onDown) onDown(nx(), ny()); }
        else        { if (onUp)   onUp(); }
      } else if (m_down && m_moved && onMove) {
        onMove(nx(), ny());
      }
      m_moved = false;
    }
  }

private:
  static constexpr int kSlots = 16;

  bool absRange(int axis, int& lo, int& hi) {
    input_absinfo ai{};
    if (ioctl(m_fd, EVIOCGABS(axis), &ai) < 0) return false;
    if (ai.maximum <= ai.minimum) return false;
    lo = ai.minimum;
    hi = ai.maximum;
    return true;
  }
  double nx() const {
    return std::clamp((m_x - m_minX) / double(m_maxX - m_minX), 0.0, 1.0);
  }
  double ny() const {
    return std::clamp((m_y - m_minY) / double(m_maxY - m_minY), 0.0, 1.0);
  }

  int m_minX = 0, m_maxX = 1, m_minY = 0, m_maxY = 1;
  int m_x = 0, m_y = 0, m_slot = 0;
  bool m_mt = false, m_down = false, m_edge = false, m_moved = false;
  bool m_active[kSlots] = {};
};

// ── Keyboard (Type Folio) and power button ───────────────────────
// Emits (name, text) matching the previous Qt key ABI: special keys get a
// name ("Backspace", "Enter", ...) and empty text; printables get an empty
// name and the character in text.
class KeyboardReader : public EvdevReader {
public:
  std::function<void(const QString& name, const QString& text)> onKey;

protected:
  void handle(const input_event& ev) override {
    if (ev.type != EV_KEY) return;
    if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
      m_shift = (ev.value != 0);
      return;
    }
    if (ev.value == 0) return;  // key release; act on press (1) + repeat (2)
    if (ev.code == KEY_CAPSLOCK) {
      if (ev.value == 1) m_caps = !m_caps;
      return;
    }

    struct Key { unsigned short code; const char* name; const char* plain; const char* shifted; };
    static const Key map[] = {
      {KEY_ESC, "Escape", "", ""},       {KEY_TAB, "Tab", "", ""},
      {KEY_ENTER, "Enter", "", ""},      {KEY_BACKSPACE, "Backspace", "", ""},
      {KEY_DELETE, "Delete", "", ""},    {KEY_HOME, "Home", "", ""},
      {KEY_END, "End", "", ""},          {KEY_LEFT, "ArrowLeft", "", ""},
      {KEY_RIGHT, "ArrowRight", "", ""}, {KEY_UP, "ArrowUp", "", ""},
      {KEY_DOWN, "ArrowDown", "", ""},   {KEY_POWER, "Power", "", ""},
      {KEY_SPACE, "", " ", " "},
      {KEY_1, "", "1", "!"}, {KEY_2, "", "2", "@"}, {KEY_3, "", "3", "#"},
      {KEY_4, "", "4", "$"}, {KEY_5, "", "5", "%"}, {KEY_6, "", "6", "^"},
      {KEY_7, "", "7", "&"}, {KEY_8, "", "8", "*"}, {KEY_9, "", "9", "("},
      {KEY_0, "", "0", ")"},
      {KEY_MINUS, "", "-", "_"},      {KEY_EQUAL, "", "=", "+"},
      {KEY_LEFTBRACE, "", "[", "{"},  {KEY_RIGHTBRACE, "", "]", "}"},
      {KEY_BACKSLASH, "", "\\", "|"}, {KEY_SEMICOLON, "", ";", ":"},
      {KEY_APOSTROPHE, "", "'", "\""},{KEY_GRAVE, "", "`", "~"},
      {KEY_COMMA, "", ",", "<"},      {KEY_DOT, "", ".", ">"},
      {KEY_SLASH, "", "/", "?"},
      {KEY_A, "", "a", "A"}, {KEY_B, "", "b", "B"}, {KEY_C, "", "c", "C"},
      {KEY_D, "", "d", "D"}, {KEY_E, "", "e", "E"}, {KEY_F, "", "f", "F"},
      {KEY_G, "", "g", "G"}, {KEY_H, "", "h", "H"}, {KEY_I, "", "i", "I"},
      {KEY_J, "", "j", "J"}, {KEY_K, "", "k", "K"}, {KEY_L, "", "l", "L"},
      {KEY_M, "", "m", "M"}, {KEY_N, "", "n", "N"}, {KEY_O, "", "o", "O"},
      {KEY_P, "", "p", "P"}, {KEY_Q, "", "q", "Q"}, {KEY_R, "", "r", "R"},
      {KEY_S, "", "s", "S"}, {KEY_T, "", "t", "T"}, {KEY_U, "", "u", "U"},
      {KEY_V, "", "v", "V"}, {KEY_W, "", "w", "W"}, {KEY_X, "", "x", "X"},
      {KEY_Y, "", "y", "Y"}, {KEY_Z, "", "z", "Z"},
    };

    for (const Key& k : map) {
      if (k.code != ev.code) continue;
      bool isLetter = (k.plain[0] >= 'a' && k.plain[0] <= 'z');
      bool upper = isLetter ? (m_shift != m_caps) : m_shift;
      if (onKey) onKey(k.name, upper ? k.shifted : k.plain);
      return;
    }
  }

private:
  bool m_shift = false, m_caps = false;
};
