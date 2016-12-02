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

_home_device = "http://192.168.4.1/espman/";

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




$("#wifipage").on("pagebeforeshow", function() {
    $('#wifinetworks-form').hide();
    $('#wifi_status').empty().append("Please wait. Scanning for WiFi Networks");

    $.post(_home_device + "data.esp", "PerformWiFiScan", function(result) {
            $('#wifi_status').empty();
            $('#wifinetworks-form').show();
            //datatosave(result);
            console.log(result);
            if (result.hasOwnProperty("networks")) {
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
                "<br>Security: " + object.encyrpted +
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



$("#save_finalpage").click(function() {
    console.log("Save"); 
    $("#final_message").empty().append("Saving Data... ");
    $.post(_home_device + "data.esp", "save", function(data) {

        if (data == "OK") {
            $("#final_message").append("Done.<br> Rebooting.  Please wait 1 min, then reconnect to Home WiFi and use URL above.");
            $("#edit_butons").hide();
            $("#launch_buttons").show();
            $("#launch_finalpage").attr("href", "http://" + _data.STA.IP + "/espman/" );
        }

        console.log("Done");
    });
});

function cancel_func() {
    console.log("Cancel");
    $.post(_home_device + "data.esp", "cancelWizard", function(e) {
    });
    $.mobile.navigate("#mainpage");
};

$("#confirmpage").on("pagebeforeshow", function() {

    $("#summary").empty();
    $("#summary").append("<br> Device Name: " + devicename + "<br>");
    $("#summary").append("SSID: " + ssid + "<br>");
    $("#summary").append("Channel: " + channel + "<br>");
    $("#summary").append("PASS: " + password + "<br>");

});



$("#confirm_endpage").on("click", function(event) {
    event.preventDefault();
    var send = { "deviceid": devicename, "ssid": ssid, "pass": password };
    var wizard = false;
    console.log("data", send);

    $.post(_home_device + "data.esp", "enterWizard", function(result) {

        if (result == "OK") {
            console.log("Device Entering Wizard mode");
            wizard = true;


            $.post(_home_device + "data.esp", { "deviceid": devicename }, function(result) {

                console.log("Devicename Set");



                $.post(_home_device + "data.esp", { "ssid": ssid, "pass": password, "STAchannel_desired": channel }, function(data, success) {


                if (_data) {

                    if (_data.STA.channel != channel) {
                        alert("Reconnect to : " + _data.AP.ssid);
                    }
                }

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
