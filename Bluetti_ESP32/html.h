#include "Arduino.h"
#include "config.h"

const char index_html[] PROGMEM = R"rawliteral(
<html>

<head>
  <title>%TITLE%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
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

    /*BtConnected Rows*/
    .btConnected {
      display:none;
    }
    [id$=btConnected]:checked + table > tbody > tr > td.btConnected{
      display:inline-flex;
    }
    
  </style>
</head>

<body>
  %AUTO_REFRESH_B%
  <table id="tblData" border="0" style="width:100%;max-width:900px">
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
        <td><b><a style='color:red' href="./update" target='_blank'>OTA Update</a></b></td>
      </tr>
      <tr>
        <td>uptime :</td>
        <td id='UPTIME'></td>
        <td>Running since: <span id='RUNNING_SINCE'>waiting ...</span></td>
      </tr>
      <tr>
        <td>uptime (d):</td>
        <td id='UPTIME_D'>waiting ...</td>
        <td>Last WebSocket message at: <span id='lastWebSocketTime'>waiting ...</span></td>
      </tr>
      <tr>
        <td>Bluetti device id:</td>
        <td>%BLUETTI_ID%</td>
        %BT_FILE_LOG%
      </tr>
      <tr>
        <td>BT connected: </td>
        <td><input id="btConnected" class='chkBtConnected' type="checkbox" onclick="return false;"></td>
        <td>
          <input id='btnInvertConnect' type="button" onclick="location.href='./btConnectionInvert';" value="Disconnect from BT">
        </td>
      </tr>
      <tr>
        <td>BT last message time:</td>
        <td id='BT_LAST_MEX_TIME'>waiting ...</td>
        <td style="vertical-align: middle;" class='btConnected'>
          <span>Ac Output: </span>
          <label class="switch">
            <input id='chk_AC_OUTPUT_ON' type="checkbox" onclick="onCheckChanged('AC_OUTPUT_ON',this);" %B_AC_OUTPUT_ON%>
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
      <tr><td colspan='4' class='btConnected'><b>Device States</b></td></tr>
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
  
  var Socket;
  
  function init() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    var timeout = setTimeout(function() {
        timedOut = true;
        
        try {
          Socket.close();
        } catch (error) {
          console.error(error);
        }

        timedOut = false;
    }, 15000);
    
    Socket.onmessage = function(event) {
      clearTimeout(timeout);
      processCommand(event);
    };
    Socket.onclose = function(e) {
      clearTimeout(timeout);
      console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
      setTimeout(function() {
        init();
      }, 1000);
    };

    Socket.onerror = function(err) {
      console.error('Socket encountered error: ', err.message, 'Closing socket');
      try {
        Socket.close();
      } catch (error) {
        console.error(error);
      }
    };
  }
  function processCommand(event) {
    var obj = JSON.parse(event.data);
    // console.log('---------');
    // console.log(event.data);

    document.getElementById('UPTIME').innerHTML = obj.UPTIME;
    document.getElementById('RUNNING_SINCE').innerHTML = obj.RUNNING_SINCE;
    document.getElementById('UPTIME_D').innerHTML = obj.UPTIME_D;

    document.getElementById('btConnected').checked = obj.B_BT_CONNECTED;
    if (obj.B_BT_CONNECTED){
        document.getElementById('btnInvertConnect').value='Disconnect from BT';
    }else{
      document.getElementById('btnInvertConnect').value='Reconnect to BT';
    }

    document.getElementById('BT_LAST_MEX_TIME').innerHTML = obj.BT_LAST_MEX_TIME;
    document.getElementById('chk_AC_OUTPUT_ON').checked = obj.B_AC_OUTPUT_ON;

    //debug infos, html elements might not exists
    try {
      document.getElementById('FREE_HEAP').innerHTML = obj.FREE_HEAP;
      document.getElementById('TOTAL_HEAP').innerHTML = obj.TOTAL_HEAP;
      document.getElementById('PERC_HEAP').innerHTML = obj.PERC_HEAP;

      document.getElementById('MAX_ALLOC').innerHTML = obj.MAX_ALLOC;
      document.getElementById('MIN_ALLOC').innerHTML = obj.MIN_ALLOC;

      document.getElementById('SPIFFS_USED').innerHTML = obj.SPIFFS_USED;
      document.getElementById('SPIFFS_TOTAL').innerHTML = obj.SPIFFS_TOTAL;
      document.getElementById('SPIFFS_PERC').innerHTML = obj.SPIFFS_PERC;
    } catch (error) {
      //it's ok
    }

    document.getElementById('lastWebSocketTime').innerHTML = obj.lastWebSocketTime;

    for (var key in obj.bluetti_state_data) {
        if (obj.bluetti_state_data.hasOwnProperty(key)) {
          //if the element desn't exists, add a new row
          var el = document.getElementById(key);
          if (el == null){
            var table = document.getElementById('tblData');
	          var rowCount = table.rows.length;
            var row = table.insertRow(rowCount);
            row.innerHTML = "<td class='btConnected'>"+key+"</td>" +
                            "<td id='"+key+"' class='btConnected'>waiting ...</td>";

            el = document.getElementById(key);
          }
          el.innerHTML = obj.bluetti_state_data[key];
        }
    }

  }
  //TODO: might be interesting in the future instead of a post call?
  // document.getElementById('BTN_1').addEventListener('click', button_1_pressed);
  // document.getElementById('BTN_2').addEventListener('click', button_2_pressed);
  // function button_1_pressed() {
  //   Socket.send('1');
  // }
  // function button_2_pressed() {
  //   Socket.send('0');
  // }

  window.onload = function(event) {
    init();
  }
</script>

</html>
)rawliteral";

const char bt_log_Link_html[] PROGMEM = R"rawliteral(
  <td><a href='./dataLog' target='_blank' class='btConnected'>Bluetti data Log</a></td>
)rawliteral";

const char debugInfos_html[] PROGMEM = R"rawliteral(      
      <tr>
        <td>Free Heap (Bytes):</td>
        <td><span id='FREE_HEAP'>waiting ...</span> of <span id='TOTAL_HEAP'>waiting ...</span> (<span id='PERC_HEAP'>waiting ...</span> %)</td>
      </tr>
      <tr>
        <td>Alloc Heap / Min Free Alloc Heap(Bytes):</td>
        <td><span id='MAX_ALLOC'>waiting ...</span> / <span id='MIN_ALLOC'>waiting ...</span></td>
      </tr>
      <tr>
        <td><a href="./debugLog" target="_blank">Debug Log</a></td>
        <td><b><a href="./clearLog" target="_blank">Clear Debug Log</a></b></td>
      </tr>
      <tr>
        <td>SPIFFS Used (Bytes):</td>
        <td><span id='SPIFFS_USED'>waiting ...</span> of <span id='SPIFFS_TOTAL'>waiting ...</span> (<span id='SPIFFS_PERC'>waiting ...</span> %)</td>
      </tr>
      <tr>
        <td colspan="3">
          <hr>
        </td>
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
    <table border="0" style="width:100%;max-width:800px">
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
        %IFTTT%
        <tr>
          <td>&nbsp;</td>
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

#ifdef IFTTT
const char IFTTT_html[] PROGMEM =R"rawliteral(
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
)rawliteral";
#endif

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