// How a task's date reads on a board card and in a list row (APP-262/263).
// Not a library: it uses the I18n and AppController singletons of the file
// that imports it.

// Whole days from `today` to `d` (negative = past); NaN for no date.
function daysFrom(d, today) {
    if (!d || !d.getTime || isNaN(d.getTime())) return NaN;
    const a = new Date(today.getFullYear(), today.getMonth(), today.getDate()).getTime();
    const b = new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime();
    return Math.round((b - a) / 86400000);
}

// The card: "сегодня" / "завтра", a weekday this week ("сб"), else "13 окт";
// a clock time after it when the date has one. Past dates are counted.
function cardText(d, hasTime, today) {
    const days = daysFrom(d, today);
    if (isNaN(days)) return "";
    const clock = hasTime ? " " + I18n.fmtTime(d) : "";
    if (days < 0) return I18n.t("task.due.overdue").arg(-days) + clock;
    if (days === 0) return I18n.t("task.due.today") + clock;
    if (days === 1) return I18n.t("task.due.tomorrow") + clock;
    if (days < 7) return I18n.dayName(d.getDay()) + clock;
    return I18n.fmtDate(d, "dayMonth") + clock;
}

// The list row, right-aligned (H2-List): "сегодня", "9 окт · 15:00" for
// tomorrow, "сб, 10 окт" further on. Quiet (Q-List, `plain`): a weekday
// alone while it is this week ("пт", "пт, 15:00"), then "пн, 12 окт".
function rowText(d, hasTime, today, plain) {
    const days = daysFrom(d, today);
    if (isNaN(days)) return "";
    const clock = hasTime ? (plain ? ", " : " · ") + I18n.fmtTime(d) : "";
    if (days === 0) return I18n.t("task.due.today") + clock;
    if (plain) {
        const toSunday = (7 - today.getDay()) % 7;
        if (days > 0 && days <= toSunday) return I18n.dayName(d.getDay()) + clock;
        if (days > 0 && days < 14) return I18n.fmtDate(d, "weekdayDay") + clock;
        return I18n.fmtDate(d, "dayMonth") + clock;
    }
    if (days === 1) return I18n.fmtDate(d, "dayMonth") + clock;
    if (days > 0 && days < 14) return I18n.fmtDate(d, "weekdayDay") + clock;
    return I18n.fmtDate(d, "dayMonth") + clock;
}

// A repeat rule in words ("каждый день", "по пятницам", "каждый месяц · 15"),
// for the ↻ mark of a recurring task on a card and a Today row (A2-25). The
// card said "daily" / "Mondays" in English in a Russian UI.
function repeatText(r) {
    const rule = String(r || "");
    if (rule.length === 0) return "";
    const interval = intervalText(rule);
    if (interval.length > 0) return interval;
    const monthly = /^every:month:(\d+)$/.exec(rule);
    if (monthly) return I18n.t("taskdoc.repeat.month") + " · " + monthly[1];
    const key = rule.replace("every:", "");
    const said = I18n.t("taskdoc.repeat." + key);
    return said.indexOf("taskdoc.") === 0 ? key : said;
}

// The rules B7 added, in words: "раз в 2 дня", "every 3 weeks", "раз в 2
// месяца · 15-го", "по пн, ср" / "every mon, wed". "" for any other rule,
// which the caller words its own way.
function intervalText(r) {
    const rule = String(r || "");
    const n = /^every:(\d+):(day|week|month)(?::(\d+))?$/.exec(rule);
    if (n) {
        const said = I18n.count(parseInt(n[1]), "repeat.everyN." + n[2]);
        return n[3] ? said + " · " + I18n.t("repeat.monthDay").arg(n[3]) : said;
    }
    const days = /^every:((?:mon|tue|wed|thu|fri|sat|sun)(?:,(?:mon|tue|wed|thu|fri|sat|sun))+)$/.exec(rule);
    if (days) {
        const js = { sun: 0, mon: 1, tue: 2, wed: 3, thu: 4, fri: 5, sat: 6 };
        const names = days[1].split(",").map(d => I18n.dayName(js[d]).toLowerCase());
        return I18n.t("repeat.onDays").arg(names.join(", "));
    }
    return "";
}

// A rule as the start of a line or a menu row: "Раз в 2 дня", "Every mon, wed".
function intervalTitle(r) {
    const s = intervalText(r);
    return s.length > 0 ? s.charAt(0).toUpperCase() + s.slice(1) : "";
}
