#pragma once
#include "agenda_ui_types.h"
class AlarmUiController {
public:
    explicit AlarmUiController(AlarmService* alarms):alarms_(alarms){}
    bool active() const {return page_!=Page::Closed;}
    bool handle(const AgendaUiAction& action, const std::optional<DateTime>& now, bool allowOpen);
    void populate(AgendaUiView& view) const;
private:
    enum class Page {Closed,List,Editor};
    void clampPage();
    AlarmService* alarms_;
    Page page_=Page::Closed;
    Alarm draft_;
    std::size_t pageIndex_=0;
    bool confirmDelete_=false;
    AlarmError error_=AlarmError::None;
};
