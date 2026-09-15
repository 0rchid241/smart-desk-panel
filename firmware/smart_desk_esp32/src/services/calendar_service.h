#pragma once
#include "../core/app_types.h"

namespace CalendarService {
void init();
bool syncCalendarEvents();
void startSyncClock();
void update(bool reconnected);
void checkScheduleReminders(bool notificationActive, NotificationHandler notify);
// Index comes from the lookup functions below. Reference is valid until sync.
const ScheduleEvent& getEvent(int index);
int getDaysUntil(const ScheduleEvent& event);
int findNextEventIndex();
int findUpcomingEventIndexByOrder(int targetOrder);
int getCalendarPageCount();
String getDDayText(int daysUntil);
String getEventDateText(const ScheduleEvent& event);
}
