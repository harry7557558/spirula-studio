#pragma once

// The trainer's VRAM forecast on screen: the out-of-memory risk tag beside the
// status strip's VRAM bar, and the card that bar shows on hover.

#include "app/TrainForecast.h"
#include "app/gui/Ui.h"

namespace gui {

// nullptr while the risk is unknown.
const spirula::i18n::Msg* oom_risk_label(spirula::OomRisk r);
ImVec4 oom_risk_color(spirula::OomRisk r);

// Memory over the run with its projection, then this run's usage by category.
// Call inside a tooltip.
void vram_forecast_card(const spirula::VramForecast& v, int total_steps);

}  // namespace gui
