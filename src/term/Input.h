#pragma once

#include <QByteArray>
#include <QKeyEvent>

namespace gt {

/// Translates a key press into the bytes a terminal program expects (xterm conventions).
/// Returns an empty array for keys that produce nothing (bare modifiers, or app shortcuts).
QByteArray encodeKey(const QKeyEvent &event, bool applicationCursorKeys);

/// Wraps pasted text for bracketed-paste mode, or normalises line breaks when it is off.
QByteArray encodePaste(const QString &text, bool bracketed);

} // namespace gt
