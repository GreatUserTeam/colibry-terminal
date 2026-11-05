#pragma once

#include "term/ColorScheme.h"

#include <QColor>
#include <QString>

namespace gt {

/// Style sheet in the Colibry look: soft rounded corners, one blue accent, flat surfaces.
QString buildStyleSheet(const ColorScheme &scheme);

/// Colour of the window chrome (tab strip, search bar, dialogs).
QColor chromeColor(const ColorScheme &scheme);

} // namespace gt
