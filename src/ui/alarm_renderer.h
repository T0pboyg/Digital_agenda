#pragma once
#include "agenda_renderer.h"
#include <optional>
namespace AlarmLayout {
constexpr UiRect Open{104,184,32,28};
constexpr UiRect Stop{62,156,116,40};
constexpr UiRect Back{46,16,30,28};
constexpr UiRect Add{164,16,30,28};
constexpr UiRect Prev{48,184,40,28};
constexpr UiRect Next{152,184,40,28};
constexpr UiRect Save{124,198,64,28};
constexpr UiRect Cancel{52,198,60,28};
constexpr UiRect DatePrevious{24,62,28,26};
constexpr UiRect DateNext{188,62,28,26};
constexpr UiRect HourUp{52,96,48,24};
constexpr UiRect HourDown{52,150,48,24};
constexpr UiRect MinuteUp{140,96,48,24};
constexpr UiRect MinuteDown{140,150,48,24};
constexpr UiRect Daily{28,178,100,18};
constexpr UiRect Enabled{136,178,76,18};
constexpr UiRect Yes{48,144,60,30};
constexpr UiRect No{132,144,60,30};
}
bool renderAlarmUi(const AgendaUiView&,DrawSurface&);
void renderDiaryStatus(const AgendaUiView&,DrawSurface&);
std::optional<AgendaUiAction> mapAlarmTouch(const AgendaUiView&,UiPoint);
