function onCheckChanged(command, chkBox) {
  var val = '0';
  if (chkBox.checked) {
    val = '1';
  }
  postCommand(command, val);
}

function postCommand(type, value) {

  const data = new URLSearchParams();
  data.append('type', type);
  data.append('value', value);

  fetch("./post", {
    method: "POST",
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: data
  }).then(res => {
    console.log("Request complete! response:", res);
  });
}

function jsCall(url, timeout) {
  fetch(url, {
    method: "GET"
  }).then(res => {
    console.log("Request complete! response:", res);
    if (timeout != null) {
      setTimeout(function () { location.reload(); }, timeout);
    }
  });
}

var Socket;

function init() {
  Socket = new WebSocket('ws://' + window.location.hostname + '/ws');
  var timeout = setTimeout(function () {
    timedOut = true;

    try {
      Socket.close();
    } catch (error) {
      console.error(error);
    }

    timedOut = false;
  }, 15000);

  Socket.onmessage = function (event) {
    clearTimeout(timeout);
    processCommand(event);
  };
  Socket.onclose = function (e) {
    clearTimeout(timeout);
    console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
    setTimeout(function () {
      init();
    }, 1000);
  };

  Socket.onerror = function (err) {
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

  document.getElementById('BT_LAST_MEX_TIME').innerHTML = obj.BT_LAST_MEX_TIME;
  document.getElementById('chk_AC_OUTPUT_ON').checked = obj.B_AC_OUTPUT_ON;
  document.getElementById('chk_DC_OUTPUT_ON').checked = obj.B_DC_OUTPUT_ON;

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

  document.getElementById('BATT_PERC_MIN').innerHTML = obj.BATT_PERC_MIN;
  document.getElementById('BATT_PERC_MAX').innerHTML = obj.BATT_PERC_MAX;
  document.getElementById('DC_INPUT_MAX').innerHTML = obj.DC_INPUT_MAX;
  document.getElementById('DC_OUTPUT_MAX').innerHTML = obj.DC_OUTPUT_MAX;
  document.getElementById('AC_INPUT_MAX').innerHTML = obj.AC_INPUT_MAX;
  document.getElementById('AC_OUTPUT_MAX').innerHTML = obj.AC_OUTPUT_MAX;

  if (obj.TEMPERATURE_CURRENT != null) {
    //show temperature row
    document.getElementsByClassName("temperature_sensor")[0].style.display = "table-row";
    document.getElementById('TEMPERATURE_CURRENT').innerHTML = obj.TEMPERATURE_CURRENT;
    document.getElementById('TEMPERATURE_MAX').innerHTML = obj.TEMPERATURE_MAX;
    document.getElementById('HUMIDITY_CURRENT').innerHTML = obj.HUMIDITY_CURRENT;
    document.getElementById('HUMIDITY_MAX').innerHTML = obj.HUMIDITY_MAX;
  }

  if (obj.CURR_EXT_VOLTAGE != null) {
    document.getElementsByClassName("ext_battery")[0].style.display = "table-row";
    document.getElementById('CURR_EXT_VOLTAGE').innerHTML = obj.CURR_EXT_VOLTAGE;
    document.getElementById('chk_PWM_SWITCH').checked = obj.B_PWM_SWITCH;
  }

  document.getElementById('lastWebSocketTime').innerHTML = obj.lastWebSocketTime;

  for (var key in obj.bluetti_state_data) {
    if (obj.bluetti_state_data.hasOwnProperty(key)) {
      //if the element desn't exists, add a new row
      var el = document.getElementById(key);
      if (el == null) {
        var table = document.getElementById('tblData');
        var rowCount = table.rows.length;
        var row = table.insertRow(rowCount);
        row.innerHTML = "<td class=\"btConnected\">" + key + "</td>" +
          "<td id=\"" + key + "\" class=\"btConnected\">waiting ...</td>";

        el = document.getElementById(key);
      }
      el.innerHTML = obj.bluetti_state_data[key];
    }
  }

  document.getElementById('btConnected').checked = obj.B_BT_CONNECTED;
  if (obj.B_BT_CONNECTED) {
    document.getElementById('btnInvertConnect').value = 'Disconnect from BT';
    document.querySelectorAll(".btConnected").forEach(el => el.className = "btConnected checked");
  } else {
    document.getElementById('btnInvertConnect').value = 'Reconnect to BT';
    document.querySelectorAll(".btConnected").forEach(el => el.className = "btConnected");
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

window.onload = function (event) {
  init();
}