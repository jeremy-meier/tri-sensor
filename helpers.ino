#include <Timezone_Generic.h>

TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};
Timezone usCentral(usCDT, usCST);

String iso8601_date() {
  return iso8601_date(now());
}

String iso8601_date(time_t) {
  String t_str = "";

  TimeChangeRule *tcr;
  time_t t_loc = usCentral.toLocal(now(), &tcr);

  t_str += String(year(t_loc));
  t_str += "-";
  t_str += format_digits(month(t_loc));
  t_str += "-";
  t_str += format_digits(day(t_loc));
  t_str += "T";
  t_str += format_digits(hour(t_loc));
  t_str += ":";
  t_str += format_digits(minute(t_loc));
  t_str += ":";
  t_str += format_digits(second(t_loc));
  t_str += format_offset(tcr->offset);

  return t_str;
}

String format_digits(int digits) {
  return (digits < 10) ? "0" + String(digits) : String(digits);
}

String format_offset(int offset) {
  if (offset == 0) {
    return "Z";
  }

  String offset_suffix;
  offset_suffix = (offset > 0) ? "+" : "-";

  int offset_hours = abs(offset) / 60;
  offset_suffix += format_digits(offset_hours);

  offset_suffix += ":";

  int offset_minutes = abs(offset) % 60;
  offset_suffix += format_digits(offset_minutes);

  return offset_suffix;
}

char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, val);
  return sout;
}
