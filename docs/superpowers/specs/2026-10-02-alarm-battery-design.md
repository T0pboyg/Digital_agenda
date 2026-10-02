# Alarm and battery completion

Continue the authorized Digital Diary work from actual user snapshot 6d0260d. This snapshot has no alarm subsystem. Prior conversational approvals supply the brief.

- Independent one-time date/time and daily alarms, three per page; create, edit, enable/disable, delete. No title or snooze. +/- hour and minute controls.
- Dedicated clock icon opens alarm list. Ringing takes priority over every agenda screen/overlay without discarding editor state. Large current time, only Διακοπή. Repeating locally generated melody through MAX98357A. Visual alarm survives audio failure.
- Persist alarms independently of agenda.bin with version, CRC, validated bounded records and atomic LittleFS write. Persist pending ringing so reset does not silently lose alarm. Stop acknowledges all currently due alarms. No Flash writes during ordinary polling.
- Daily fires once per civil date; repeated DST hour does not ring twice. A skipped spring hour rings on first poll afterwards. One-time alarms overdue while powered off ring on next valid clock; daily alarms catch up today only. Reject past enabled one-time edits.
- Select earliest reminder/alarm/policy timer. Keep existing deep/light sleep, block sleep during ringing, retain uncommitted editors through light sleep. Cap alarm-enabled sleep at 60 seconds to re-evaluate civil time across DST. Actual wake uses existing Greece-aware clock.
- Status icon and estimated LiPo percentage from calibrated GPIO1 ADC voltage *3 (Waveshare 200k/100k divider). Smooth measurements, unknown on implausible voltage; do not claim charge detection. Sample while awake, no battery-specific wakeups.
- Small HH:MM and battery on main screens, fit 240px round display. Keep old editor layouts. Up to 24 alarms as a bounded memory/storage limit.
- ESP32 audio GPIO15 BCLK, GPIO16 LRCLK, GPIO17 DIN; optional shutdown GPIO18 is not enabled without wiring confirmation. Pins are centralized. Electrical assembly and audible verification require user's physical board.
- Do not erase or upload filesystem; preserve agenda records and firmware partition configuration.
