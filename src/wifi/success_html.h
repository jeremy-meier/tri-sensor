const char SUCCESS_HTML[] PROGMEM = R"=====(HTTP/1.1 200 OK
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
    <h1>Tri-Sensor Setup Complete!</h1>
    <p>You may now close this window.</p>
  </body>
</html>
)=====";