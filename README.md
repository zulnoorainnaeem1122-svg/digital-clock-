A digital clock system uses an RTC module to keep accurate real-time data, 
displayed on an LCD using I2C communication. Four push buttons are used: 
one for setting/scrolling time and date, second for increasing values,
third for decreasing values, and fourth for alarm settings. 
When the alarm button is pressed, the user can adjust hours, minutes, 
and seconds within a limited time window. After setting, pressing the alarm button again saves it,
and the system checks continuously for a match with current time. When matched, the buzzer activates
for 10 seconds or stops early if the alarm button is pressed again.
<img width="1204" height="1600" alt="image" src="https://github.com/user-attachments/assets/605445b9-ff84-4528-bb1d-082f994cc7bb" />
