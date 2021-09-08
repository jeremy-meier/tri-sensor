const char FORM_HTML[] PROGMEM = R"=====(HTTP/1.1 200 OK
Content-Type: text/html

<!DOCTYPE HTML>
<html lang="en">
  <head>
    <title>Tri-Sensor WiFi Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="60">
    <style>
      .form-element { margin: 10px 15px; }
      .form-element label { width: 150px; display: inline-block; text-align: right; }
      .form-element input { border-radius: 4px; border: none; padding: 4px 8px; }
      .form-element .submit { margin-left: 154px; padding: 5px 10px; -webkit-appearance: none; border: none; background-color: #ddd; border-radius: 4px; }
      .form-element:last-child { margin-top: 20px; }
      body { background-color: steelblue; font-family: Veranda, Helvetica, sans-serif; padding: 20px;}
    </style>
  </head>
  <body>
    <h1>Tri-Sensor</h1>
    <p>Enter device configuration below:</p>
    <form method="POST" action="checkpass.html">
      <div class="form-element">
        <label for="wifi_ssid">Wifi Network SSID:</label>
        <input type="text" name="wifi_ssid" id="wifi_ssid" maxlength="31" required />
      </div>
      <div class="form-element">
        <label for="wifi_pass">Wifi Password:</label>
        <input type="password" name="wifi_pass" id="wifi_pass" maxlength="31" required />
      </div>
      <div class="form-element">
        <label for="mqtt_host">MQTT Host:</label>
        <input type="text" name="mqtt_host" id="mqtt_host" maxlength="127" required />
      </div>
      <div class="form-element">
        <label for="mqtt_port">MQTT Port:</label>
        <input type="number" name="mqtt_port" id="mqtt_port" maxlength="8" min="1024" value="1883" required />
      </div>
      <div class="form-element">
        <label for="mqtt_user">MQTT User:</label>
        <input type="text" name="mqtt_user" id="mqtt_user" maxlength="31" required />
      </div>
      <div class="form-element">
        <label for="mqtt_pass">MQTT Password:</label>
        <input type="password" name="mqtt_pass" id="mqtt_pass" maxlength="31" required />
      </div>
      <div class="form-element">
        <label for="device_name">Device Name:</label>
        <input type="text" name="device_name" id="device_name" maxlength="31" required />
      </div>
      <div  class="form-element">
        <input class="submit" type="submit" name="action" value="Submit" />
      </div>
    </form>
  </body>
</html>
)=====";