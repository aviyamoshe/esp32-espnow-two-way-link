#include <Arduino.h>
#include <WiFi.h>

void setup() {
  // פותחים את חלון התקשורת עם המחשב
  Serial.begin(115200);

  // מדליקים את הרדיו במצב תחנה, בלי להתחבר לשום רשת
  WiFi.mode(WIFI_STA);
}

void loop() {
  // מדפיסים את הכתובת, עם שם הבקר כדי לא להתבלבל
  Serial.print("CONTROLLER MAC: ");
  Serial.println(WiFi.macAddress());

  // מחכים שנייה, וחוזרים
  delay(1000);
}