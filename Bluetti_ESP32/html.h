#include "Arduino.h"

const char index_html[] PROGMEM = R"rawliteral(
<html>

<head>
  <title>%TITLE%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <!--<link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">-->
  %AUTO_REFRESH_H%
  <style>
    .switch {
      position: relative;
      display: inline-block;
      width: 32px;
      height: 21px;
    }

    .switch input { 
      opacity: 0;
      width: 0;
      height: 0;
    }

    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      -webkit-transition: .4s;
      transition: .4s;
    }

    .slider:before {
      position: absolute;
      content: "";
      height: 15px;
      width: 15px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      -webkit-transition: .4s;
      transition: .4s;
    }

    input:checked + .slider {
      background-color: #689f38;
    }

    input:focus + .slider {
      box-shadow: 0 0 1px #689f38;
    }

    input:checked + .slider:before {
      -webkit-transform: translateX(13px);
      -ms-transform: translateX(13px);
      transform: translateX(13px);
    }

    /* Rounded sliders */
    .slider.round {
      border-radius: 34px;
    }

    .slider.round:before {
      border-radius: 50%;
    }

    .btDisconnect{
      display:none;
    }
    .btDisconnect.checked{
      display:block;
    }
  </style>
</head>

<body>
  %AUTO_REFRESH_B%
  <table border="0" style="width:100%">
    <tbody>
      <tr>
        <td>host:</td>
        <td>%HOST%</td>
        <td><a href="./rebootDevice">Reboot this device</a></td>
      </tr>
      <tr>
        <td>SSID:</td>
        <td>%SSID%</td>
        <td><a href="./resetWifiConfig">Reset WIFI config</a></td>
      </tr>
      <tr>
        <td>WiFiRSSI:</td>
        <td>%WiFiRSSI%</td>
        <td><b><a href="./config">Change Configuration</a></b></td>
      </tr>
      <tr>
        <td>MAC:</td>
        <td>%MAC%</td>
      </tr>
      <tr>
        <td>uptime :</td>
        <td>%UPTIME%</td>
        <td>Running since: %RUNNING_SINCE%</td>
      </tr>
      <tr>
        <td>uptime (d):</td>
        <td>%UPTIME_D%</td>
      </tr>
      <tr>
        <td>Bluetti device id:</td>
        <td>%BLUETTI_ID%</td>
        %BT_FILE_LOG%
      </tr>
      <tr>
        <td>BT connected: </td>
        <td><input id="btConnected" type="checkbox" onclick="return false;" %B_BT_CONNECTED%></td>
        <td>
          <input type="button" class="btDisconnect %B_BT_CONNECTED%" onclick="location.href='./btDisconnect';" value="Disconnect from BT">
        </td>
      </tr>
      <tr>
        <td>BT last message time:</td>
        <td>%BT_LAST_MEX_TIME%</td>
        <td style="display: inline-flex;vertical-align: middle;">
          <span>Ac Output: </span>
          <label class="switch">
            <input type="checkbox" onclick="onCheckChanged('AC_OUTPUT_ON',this);" %B_AC_OUTPUT_ON%>
            <span class="slider round"></span>
          </label>
        </td>
      </tr>
      <tr>
        <td colspan="3">
          <hr>
        </td>
      </tr>
      %DEBUG_INFOS%
      %DEVICE_STATES_HEADER%
      %DEVICE_STATES_ROWS%
    </tbody>
  </table>
</body>

<script>
  function onCheckChanged(command,chkBox){
    var val = '0';
    if (chkBox.checked){
      val = '1';
    }
    postCommand(command,val);
  }
  function postCommand(type, value) {

    const data = new URLSearchParams();
    data.append('type', type);
    data.append('value', value);
    
    fetch("./post", {
      method: "POST",
      headers: {'Content-Type': 'application/x-www-form-urlencoded'}, 
      body: data
    }).then(res => {
      console.log("Request complete! response:", res);
    });
  }
</script>

</html>
)rawliteral";

const char bt_log_Link_html[] PROGMEM = R"rawliteral(
  <td><a href='./dataLog' target='_blank'>Bluetti data Log</a></td>
)rawliteral";

const char debugInfos_html[] PROGMEM = R"rawliteral(      
      <tr>
        <td>Free Heap (Bytes):</td>
        <td>%FREE_HEAP% of %TOTAL_HEAP% (%PERC_HEAP% %)</td>
      </tr>
      <tr>
        <td>Alloc Heap / Min Free Heap(Bytes):</td>
        <td>%MAX_ALLOC% / %MIN_ALLOC%</td>
      </tr>
      <tr>
        <td><a href="./debugLog" target="_blank">Debug Log</a></td>
        <td><b><a href="./clearLog" target="_blank">Clear Debug Log</a></b></td>
      </tr>
      <tr>
        <td>SPIFFS Used (Bytes):</td>
        <td>%SPIFFS_USED% of %SPIFFS_TOTAL% (%SPIFFS_PERC% %)</td>
      </tr>
      <tr>
        <td colspan="3">
          <hr>
        </td>
      </tr>
)rawliteral";

const char deviceState_header_html[] PROGMEM = R"rawliteral(
  <tr><td colspan='4'><b>Device States</b></td></tr>
)rawliteral";

const char deviceState_row_html[] PROGMEM = R"rawliteral(
      <tr>
        <td>%STATE_NAME%:</td>
        <td>%STATE_VALUE%</td>
      </tr>
)rawliteral";

const char config_html[] PROGMEM = R"rawliteral(
<html>

<head>
  <title>%TITLE% Config</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <!--<link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">-->
</head>

<body onLoad="onLoad();">

  <b><a href="./">Home</a></b>
  <form action="/config" method="POST">
    <span style='font-weight:bold;color:red;%PARAM_SAVED%'>Configuration Saved.
      <span id='restartRequired' style="%RESTART_REQUIRED%">Restart required (will be done in 2 second)</span>
    </span><br/><br/>
    <table border="0" style="width:100%">
      <tbody>
        <tr>
          <td>Bluetti device id:</td>
          <td><input type="text" size="40" name="bluetti_device_id" value="%BLUETTI_DEVICE_ID%"></td>
          <td><a href="./resetConfig" target="_blank" style="color:red">reset FULL configuration</a></td>
        </tr>
        <tr>
          <td>&nbsp;</td>
        </tr>
        <tr>
          <td>Available Wifi Networs:</td>
          <td><a href="./config">Refresh</a></td>
        </tr>
        %WIFI_SCAN%
        <tr>
          <td>&nbsp;</td>
        </tr>
        <tr>
          <td>Start in AP Mode:</td>
          <td><input type="checkbox" name="APMode" value="APModeBool" %B_APMODE%></td>
        </tr>
        <tr>
          <td>Wifi SSID:</td>
          <td><input type="text" size="50" name="ssid" value="%SSID%"></td>
        </tr>
        <tr>
          <td>Wifi PWD:</td>
          <td><input type="password" size="50" name="password" value="%PASSWORD%"></td>
        </tr>
        <tr>
          <td>Use IFTT:</td>
          <td><input type="checkbox" name="useIFTT" value="useIFTTBool" %B_USE_IFTT%></td>
        </tr>
        <tr class='showiftt'>
          <td>IFTT Key:</td>
          <td><input type="text" size="25" name="IFTT_Key" value="%IFTT_KEY%"></td>
        </tr>
        <tr class='showiftt'>
          <td>IFTT Event - Low Battery (empty to not trigger the event):</td>
          <td><input type="text" size="25" name="IFTT_Event_low" value="%IFTT_EVENT_LOW%"></td>
        </tr>
        <tr class='showiftt'>
          <td>IFTT Low Battery percentage:</td>
          <td><input type="number" placeholder="1.0" step="1" min="0" max="100" name="IFTT_low_bl" value="%IFTT_LOW_BL%"></td>
        </tr>
        <tr class='showiftt'>
          <td>IFTT Event - High Battery (empty to not trigger the event):</td>
          <td><input type="text" size="25" name="IFTT_Event_high" value="%IFTT_EVENT_HIGH%"></td>
        </tr>
        <tr class='showiftt'>
          <td>IFTT high Battery percentage:</td>
          <td><input type="number" placeholder="1.0" step="1" min="0" max="100" name="IFTT_high_bl" value="%IFTT_HIGH_BL%"></td>
        </tr>
        <tr>
          <td>&nbsp;</td>
        </tr>
        <tr>
          <td>Home Page Auto Refresh (sec, if 0 = No AutoRefresh):</td>
          <td><input type="number" placeholder="1.0" step="1" min="0" max="3600" name="homeRefreshS" value="%HOME_REFRESH_S%"></td>
        </tr>
        <tr>
          <td>Show Debug Infos (FreeHeap, debugLog Link, etc...):</td>
          <td><input type="checkbox" name="showDebugInfos" value="showDebugInfosBool" %B_SHOW_DEBUG_INFOS%></td>
        </tr>
        <tr>
          <td>Enable Debug logging to File:</td>
          <td><input type="checkbox" name="useDbgFilelog" value="useDbgFilelogBool" %B_USE_DBG_FILE_LOG%></td>
        </tr>
        <tr>
          <td>&nbsp;</td>
        </tr>
        <tr>
          <td>Bluetti Data Log Auto Start (HH:MM) (empty=no start):</td>
          <td><input type="text" size="5" name="BtLogTime_Start" value="%BTLOGTIME_START%"></td>
        </tr>
        <tr>
          <td>Bluetti Data Log Auto Stop (HH:MM) (empty=no stop):</td>
          <td><input type="text" size="5" name="BtLogTime_Stop" value="%BTLOGTIME_STOP%"></td>
        </tr>
        <tr>
          <td>&nbsp;</td>
        </tr>
        <tr>
          <td>Clear All Logs and Reboot:</td>
          <td><input type="checkbox" name="clrSpiffOnRst" value="clrSpiffOnRstBool" %B_CLRSPIFF_ON_RST%></td>
        </tr>
      </tbody>
    </table><input type="submit" value="Save">
  </form>
  <script>
    function onLoad(){
      if (document.getElementById('restartRequired').style.display != 'none'){
        setTimeout(function() { location.href = './'; }, 2000);
      }
    }
  </script>
</body>

</html>
)rawliteral";

const char wifiNoNet_html[] PROGMEM = R"rawliteral(
  <tr><td>No networks found. Refresh to scan again.</td></tr>
)rawliteral";

const char wifiRow_html[] PROGMEM = R"rawliteral(
  <tr class='wifiNetwork'>
    <td colspan="3"><a href="#"
        onclick="document.getElementsByName('ssid')[0].value=this.getAttribute('data-ssid');"
        data-ssid="%CURR_SSID%">%CURR_SSID%</a></td>
  </tr>
)rawliteral";