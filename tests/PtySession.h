#pragma once

#include "pty/Pty.h"
#include "term/Parser.h"
#include "term/Screen.h"

#include <QtTest>

using namespace gt;

namespace testing {

/// A PTY plus an emulator, driven by the event loop.
struct Session {
    Screen screen{80, 24, 1000};
    Parser parser{screen};
    Pty pty;
    QByteArray raw;
    bool done = false;
    int exitCode = -1;
    bool crashed = false;

    explicit Session(int cols = 80, int rows = 24)
    {
        screen.resize(cols, rows);
        screen.setReplyHandler([this](const QByteArray &b) { pty.write(b); });
        QObject::connect(&pty, &Pty::dataReceived, [this](const QByteArray &d) {
            raw += d;
            parser.feed(d);
        });
        QObject::connect(&pty, &Pty::finished, [this](int code, bool c) {
            done = true;
            exitCode = code;
            crashed = c;
        });
    }
    QString text() const
    {
        QString out;
        for (int i = 0; i < screen.totalLines(); ++i)
            out += screen.lineText(i) + '\n';
        return out;
    }
    bool waitFor(const QString &needle, int ms = 5000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            if (text().contains(needle))
                return true;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        return text().contains(needle);
    }
    bool waitFinished(int ms = 5000)
    {
        QElapsedTimer t;
        t.start();
        while (!done && t.elapsed() < ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        return done;
    }
};

} // namespace testing

using testing::Session;
