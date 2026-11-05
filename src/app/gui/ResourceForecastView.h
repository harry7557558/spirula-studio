#pragma once

// A ResourceForecast on screen: the risk tag, a bar per resource with its
// projected peak, and the card each shows on hover with the history and the
// forecast. The trainer's VRAM readout and every job's memory strip draw
// through here, so they read the same.

#include "app/ResourceForecast.h"
#include "app/gui/Ui.h"

#include <string>

namespace gui {

ImVec4 oom_risk_color(spirula::OomRisk r);
// nullptr while the risk is unknown.
const spirula::i18n::Msg* memory_risk_label(spirula::OomRisk r);

// Others at the bottom, ours stacked on them, the projection as its 95% band --
// or one level across the chart when the job planned its peak -- and the
// ceiling in red. x runs from 0 to `x_end`, labelled at the right end.
void forecast_chart(const spirula::ResourceForecast& f, double x_end,
                    const std::string& x_end_label);
// In use, projected peak, what is within reach and the chance of running out,
// in the risk's colour.
void forecast_summary(const spirula::ResourceForecast& f);
void forecast_legend(const spirula::i18n::Msg& run);

// The risk tag, then a RAM and a VRAM bar right-aligned in [x0, x0 + avail];
// hovering any of them shows both charts under `help`. Charts run on seconds.
void memory_strip(const spirula::ResourceForecast& ram, const spirula::ResourceForecast& vram,
                  const spirula::i18n::Msg& help, float x0, float avail);

}  // namespace gui
