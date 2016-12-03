function getBaseUrl() {
    var re = new RegExp(/^.*\//);
    var link = String(re.exec(window.location.href));
    if (link.endsWith("espman/") === false) {
        link = link + "espman/";
    }
    console.log("link = " + link);
    return link;
}

var _home_device = getBaseUrl();
var _networks;
var _data = null;
var _sendData = Array();
var devicename = "";
var ssid = "";
var channel = 1;
var password = "";


//_home_device = "http://192.168.1.240/espman/"; // tvlights

//_home_device = "http://192.168.4.1/espman/";

// $( ".wifiradio" ).contextmenu(function(event) {
//   event.preventDefault();
//   console.log("R click");  
//   WiFiMoreinfo();
// });




$("#mainpage").on("pagebeforeshow", function() {

    $('#beginconfig_btn').hide();

    $("#intro").empty().append("Connecting... ").css("color", "blue");
    $.post(_home_device + "data.esp", "generalpage", function(data) {
            console.log("Request for Data Sent");
        }).success(function(e) {
            _data = e;
            console.log("data", _data);
            //$("#intro").append(" Success").css("color", "green");
            $("#intro").empty();
            $('#beginconfig_btn').show();
        })
        .error(function() {
            $("#intro").append(" Error").css("color", "red");;
        })
        .complete(function() {});


});



$("#namepage").on("pagebeforeshow", function() {

    console.log("namepage show");

    if (_data) {

        if (_data.hasOwnProperty("General") && _data.General.hasOwnProperty("deviceid")) {

            $("#device-name").val(_data.General.deviceid);
            console.log("device name there: " + _data.General.deviceid);
        }


    } else {
        console.log("_data empty");
    }


});

$("#next_namepage").on("click", function(event) {
    event.preventDefault();

    if ($("#device-name").val().length > 0) {
        devicename = $("#device-name").val()
        console.log("deviceid = " + devicename);
        $.mobile.navigate($(this).attr("href"));
    };


});

$("#ssid-1-rescan").on("click", function(event) {
    event.preventDefault();
    $('#wifi_status').empty().append("Please Wait. ");
    StartWifiScan();


});






function StartWifiScan() {
    $('#wifinetworks-form').hide();


    $.post(_home_device + "data.esp", "PerformWiFiScan", function(result) {


            if (result.hasOwnProperty("scan")) {

                if (result.scan == "started") {
                    $('#wifi_status').append("Scanning for WiFi Networks");
                } else if (result.scan == "scanning") {
                    $('#wifi_status').append(".");
                }

                setTimeout(StartWifiScan, 500);

            } else if (result.hasOwnProperty("networks")) {

                console.log("wifiScanResults", result);
                $('#wifi_status').empty();
                $('#wifinetworks-form').show();

                _networks = result.networks;
                $("#wifinetworks-data").empty();
                $("#wifinetworks-data").append("<div>");
                $.each(_networks, function(i, object) {
                    var isconnected = "";
                    if (object.connected == "true") isconnected = "checked=\"checked\"";
                    $("#wifinetworks-data").append("<input oncontextmenu=\"WiFiMoreinfo()\"  type=\"radio\" name=\"ssid\" class=\"wifiradio\" id=\"radio-choice-v-" + i + "a\" value=\"" +
                        object.ssid + "\"" + isconnected + "><label for=\"radio-choice-v-" + i + "a\">" + object.ssid + "</label>");
                });
                $("#wifinetworks-data").append("</div>");
                $("#wifinetworks-data").enhanceWithin();


            }


        }).success(function() {
            //$("#status").empty().append("Connected").css("color", "green");
        })
        .error(function() {
            //$("#status").empty().append("Not Connected").css("color", "red");
        })
        .complete(function() {});
}




$("#wifipage").on("pagebeforeshow", function() {

    $('#wifi_status').empty().append("Please Wait. ");
    StartWifiScan();

});

$("#next_wifipage").on("click", function(event) {
    event.preventDefault();

    if ($('.wifiradio:checked:visible').val()) {
        ssid = $('.wifiradio:checked:visible').val();
        if (ssid.length) {
            console.log("ssid", ssid);
            password = $("#pass-1").val();


            if (_networks) {

                $.each(_networks, function(i, object) {

                    if ($('.wifiradio:checked:visible').val() === object.ssid) {
                        channel = object.channel;
                        console.log("channel match found");
                        return;
                    }

                });


            } else {
                console.log("_networks = null");
            }

            $.mobile.navigate($(this).attr("href"));
        }
    } else {
        console.log("Invalid Selection");
    }

});


function WiFiMoreinfo() {
    radioanswer = $('.wifiradio:checked').val();

    $.each(_networks, function(i, object) {
        if (object.ssid === radioanswer) {
            $("#wifiinsert").empty();
            $("#wifiinsert").append(
                "<h3 class=\"centerwrapper\">" + radioanswer + "</h3>" +
                "<div class=\"popup\">" +
                "<br>Connected: " + ((object.connected) ? "Yes" : "No") +
                "<br>RSSI: " + object.rssi +
                "<br>Channel: " + object.channel +
                "<br>Security: " + object.enc +
                "<br>BSSID: " + object.BSSID +
                "</div>"
            );
            $("#wifiinsert").popup("open");
        }
    });
}

$("#ssid-1-moreinfo").click(function() {
    WiFiMoreinfo();
});



function checkForReboot() {

    $.post("http://" + _data.STA.IP + "/espman/" + "data.esp", "generalpage", function(data) {

            $("#launch_buttons").show();


        }).success(function() {
            //$("#status").empty().append("Connected").css("color", "green");
        })
        .error(function() {
            //$("#status").empty().append("Not Connected").css("color", "red");
        })
        .complete(function() {});

}

$("#save_finalpage").click(function() {
    console.log("Save");
    $("#final_message").empty().append("Saving Data... ");
    $.post(_home_device + "data.esp", "save", function(data) {

        if (data == "OK") {
            $("#final_message").append("Done.<br>Please Reconnect to your WiFi and wait for the Launch Button to appear.");
            $("#edit_butons").hide();
            $("#launch_finalpage").attr("href", "http://" + _data.STA.IP + "/");

            setTimeout(function() {
                setInterval(checkForReboot, 1000);
            }, 5000);

        }

        console.log("Done");
    });
});

function cancel_func() {
    console.log("Cancel");
    $.post(_home_device + "data.esp", "cancelWizard", function(e) {});
    $.mobile.navigate("#mainpage");
};

$("#confirmpage").on("pagebeforeshow", function() {

    $("#summary").empty();
    $("#summary").append("<br> Device Name: " + devicename + "<br>");
    $("#summary").append("SSID: " + ssid + "<br>");
    $("#summary").append("Channel: " + channel + "<br>");
    $("#summary").append("PASS: " + password + "<br>");

});

function sendSettings() {

    $.post(_home_device + "data.esp", { "deviceid": devicename }, function(result) {

        console.log("Devicename Set");

        $.post(_home_device + "data.esp", { "ssid": ssid, "pass": password, "STAchannel_desired": channel }, function(data, success) {

            console.log("Setting Wifi Networks");

            if (data == "accepted") {
                console.log("WiFi Data Accepted Applying");
                var startTime = new Date().getTime();
                $.mobile.loading('show');
                var timer = setInterval(function() {
                    console.log("checking");

                    $.post(_home_device + "data.esp", "WiFiresult", function(data) {
                        console.log("WiFiResult", data);

                        if (data > 1) {
                            //if (data == "1") alert(data + " :Waiting for IP Address");
                            if (data == "2") alert(data + " :ERROR Reverted to previous settings");
                            if (data == "3") alert(data + " :Settings applied: NOT CONNECTED");
                            $.mobile.loading('hide');
                            clearInterval(timer);

                            if (data == "4") {
                                $.post(_home_device + "data.esp", "generalpage", function(data) {
                                    _data = data;
                                    console.log("data", data);
                                    $.mobile.navigate("#finalpage");
                                });
                            }
                        }
                        //refreshAPlist();
                    }, "text").error(function(e) {
                        console.log("ERROR", e);
                    });

                    if (new Date().getTime() - startTime > 65000) {
                        $.mobile.loading('hide');
                        alert("TIMEOUT: NO RESPONSE FROM ESP");
                        clearInterval(timer);
                        return;
                    }

                }, 5000);
            }
        }, "text");

    });
}


$("#confirm_endpage").on("click", function(event) {
    event.preventDefault();
    var send = { "deviceid": devicename, "ssid": ssid, "pass": password };
    var wizard = false;
    var waiting = false;
    console.log("data", send);

    $.post(_home_device + "data.esp", "enterWizard", function(result) {

        if (result == "OK") {
            console.log("Device Entering Wizard mode");
            wizard = true;


            if (_data) {

                if (_data.STA.channel != channel) {
                    waiting = true;
                    //alert("Reconnect to : " + _data.AP.ssid);

                    //("#proceedButton").hide(); 
                    $("#popuppara").empty().append(_data.AP.ssid);
                    $("#popupWiFiChannel").popup("open");

                }
            }

            if (waiting == false) {
                sendSettings(); 
            }



        } else {
            console.log("Wizard Error: " + result);
            wizard = false;
        }

    });

});


$("#finalpage").on("pagebeforeshow", function() {

    if (_data) {

        $("#final_data").empty().append("Connected to " + _data.STA.connectedssid + "  (<a href=\"http://" + _data.STA.IP + "/espman/\">" + _data.STA.IP + "</a>)");

    }

    $("#launch_buttons").hide();

});
