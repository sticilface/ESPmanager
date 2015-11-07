$(document).on("pagecreate", "#wifipage", function() {

    // Variables

    var globalwifi;
    getWiFiVars(false);

    setTimeout( function() { getWiFiVars(true); }  , 1000);

    $("#flip-dhcp").change(function() {
        stationboxes();
    });
    $("#flip-STA").change(function() {
        stationboxes();
    });


    $("#flip-AP").change(function() {
        if ($(this).val() == "on") {
            $("#APform :text").textinput('enable');
            $("#flip-AP-hidden").flipswitch('enable');
        };
        if ($(this).val() == "off") {
            $("#APform :text").textinput('disable');
            $("#flip-AP-hidden").flipswitch('disable');
        };
    });

    $("#STA_settings_div").click(function() {
        getWiFiVars(false);
    });
    $("#AP_settings_div").click(function() {
        getWiFiVars(false);
    });
    $("#ssid-1-rescan").click(function() {
        getWiFiVars(true);
    });
    $("#ssid-1-moreinfo").click(function() {
        WiFiMoreinfo();
    });
    $("#ssid-1-submit").click(function() {
        submitnewssid();
    });
    $("#rebootbutton").click(function() {
        $.post("data.esp", "reboot");
    });
    $("#resetwifi").click(function() {
        $.post("data.esp", "resetwifi");
    });
    $("#refreshdata").click(function() {
        getWiFiVars(false);
    });

    //$( document ).delegate("#wifipage", "pageinit", function() {
    //alert('A page with an id of "aboutPage" was just created by jQuery Mobile!');

    $("#general-1-submit").click(function() { // this is being used by the front page
        $.post("data.esp", $(this.form).serialize());
        var val = $("#device-name").val();
        $("#device-name").val("").attr("placeholder", val).blur();
        //return false;
    });

    $(".sendsubmit").click(function() {
        $.post("data.esp", $(this.form).serialize());
        $('[data-role="popup"]').popup("close");
        setTimeout(function() {
            getWiFiVars(false);
        }, 5000);
    });

    $('form').submit(function() {
        return false;
    });


    function getWiFiVars(scan) {
        var request;
        if (scan) {
            request = "data.esp?plain=PerformWiFiScan"
        };
        if (!scan) {
            request = "data.esp?plain=WiFiDetails";
        };
        $.getJSON(request, function(result) {
            globalwifi = result;

            if ("networks" in result) {
                staticwifi = result.networks;
                $("#wifinetworks-data").empty();
                $("#wifinetworks-data").append("<legend>Select WiFi Network:</legend>");
                $.each(result.networks, function(i, object) {
                    var isconnected = " ";
                    if (object.connected == true) isconnected = "checked=\"checked\"";
                    $("#wifinetworks-data").append("<input class = \"wifiradio\" type=\"radio\" name=\"ssid\" id=\"radio-choice-v-" + i + "a\" value=\"" +
                        object.ssid + "\"" + isconnected + "><label for=\"radio-choice-v-" + i + "a\">" + object.ssid + "</label>");
                });
                $("#wifinetworks-data").enhanceWithin();
            };
            //};  / end of if for data test

            if (result.STA.dhcp == true) {
                $('#flip-dhcp').val('on').flipswitch('refresh');
            };
            if (result.STA.dhcp == false) {
                $('#flip-dhcp').val('off').flipswitch('refresh');
            };

            if (result.STA.state == true) {
                $("#flip-STA").val('on').flipswitch('refresh');
            };
            if (result.STA.state == false) {
                $("#flip-STA").val('off').flipswitch('refresh');
            };

            $("#STA_state").empty().append((result.STA.state)? "State: Enabled":"State: Disabled");

            $("#STA_ip").empty().append("IP: " + result.STA.IP);
            $("#text-STA-set-ip").val(result.STA.IP);
            $("#STA_gateway").empty().append("Gateway: " + result.STA.gateway);
            $("#text-STA-set-gw").val(result.STA.gateway);
            $("#STA_subnet").empty().append("Subnet: " + result.STA.subnet);
            $("#text-STA-set-sn").val(result.STA.subnet);

            // DNS.. no functions in espWiFi lib to store it yet... 
            // $("#STA_dns").empty().append("DNS: " + result.DNS.subnet);
            // $("#text-DNS-set-sn").val(result.DNS.subnet);          
            if (result.AP.state == true) {
                $('#flip-AP').val('on').flipswitch('refresh');
            };
            if (result.AP.state == false) {
                $('#flip-AP').val('off').flipswitch('refresh');
            };

            if (result.AP.hidden == true) {
                $('#flip-AP-hidden').val('on').flipswitch('refresh');
            };
            if (result.AP.hidden == false) {
                $('#flip-AP-hidden').val('off').flipswitch('refresh');
            };

            $("#AP_hidden").empty().append((result.AP.hidden)? "Hidden: Yes": "Hidden: No");

            $("#STA_mac").empty().append("MAC: " + result.STA.MAC);
            $("#text-STA-set-mac").val(result.STA.MAC);

            $("#AP_state").empty().append((result.AP.state)? "State: Enabled":"State: Disabled");

            $("#AP_ssid").empty().append("SSID: " + result.AP.ssid);
            $("#text-AP-set-ssid").val(result.AP.ssid);

            $("#AP_pass").empty().append("Password: " + result.AP.password);
            $("#text-AP-set-pass").val(result.AP.pass);

            $("#AP_ip").empty().append("IP: " + result.AP.IP);
            $("#text-AP-set-ip").val(result.AP.IP);

            $("#AP_channel").empty().append("Channel: " + result.AP.channel);
            $("#text-AP-set-channel").val(result.AP.channel);

            $("#AP_mac").empty().append("MAC: " + result.AP.MAC);
            $("#text-AP-set-mac").val(result.AP.MAC);
            
            //  Update slider elements etc.. 

            if ($("#flip-AP").val() == "on") {
                $("#APform :text").textinput('enable');
                $("#flip-AP-hidden").flipswitch('enable');
            };
            if ($("#flip-AP").val() == "off") {
                $("#APform :text").textinput('disable');
                $("#flip-AP-hidden").flipswitch('disable');
            };
            if ($("#flip-dhcp").val() == "on") {
                $("#STAform :text").textinput('disable');
            };
            if ($("#flip-dhcp").val() == "off") {
                $("#STAform :text").textinput('enable');
            };
            if ($("flip-STA").val() == "on") {
                $("#flip-dhcp").flipswitch('enable');
            };
            if ($("flip-STA").val() == "off") {
                $("#flip-dhcp").flipswitch('disable');
            }; //.slider('disable');

            $("#wifipage").enhanceWithin();

            //  update general settings

            $("#device-name").val("").attr("placeholder", result.general.deviceid).blur();
            if (result.general.OTAenabled == true) {
                $("#flip-otaenable").val('on').flipswitch('refresh');
            } else {
                $("#flip-otaenable").val('off').flipswitch('refresh');
            }

        });
    }


    function refreshAPlist() {
        $.getJSON("data.esp?plain=PerformWiFiScan", function(result) {
            staticwifi = result.networks;
            $("#wifinetworks-data").empty();
            $("#wifinetworks-data").append("<div>");
            $.each(result.networks, function(i, object) {
                var isconnected = "";
                if (object.connected == "true") isconnected = "checked=\"checked\"";
                $("#wifinetworks-data").append("<input type=\"radio\" name=\"ssid\" id=\"radio-choice-v-" + i + "a\" value=\"" +
                    object.ssid + "\"" + isconnected + "><label for=\"radio-choice-v-" + i + "a\">" + object.ssid + "</label>");
            });
            $("#wifinetworks-data").append("</div>");
            $("#wifinetworks-data").enhanceWithin();
        });
    }

    function submitnewssid() {

        $.post("data.esp", $("#wifinetworks-form,#removesaftey-1").serialize(), function(data, success) {
            if (data == "accepted") {
                var startTime = new Date().getTime();
                $.mobile.loading('show');
                timer = setTimeout(function() {
                    $.post("data.esp", "WiFiresult", function(data, success2) {
                        if (success2) {
                            if (data == "1") alert(data + " :WiFi Settings Sucessfully Applied");
                            if (data == "2") alert(data + " :ERROR Reverted to previous settings");
                            if (data == "3") alert(data + " :Settings applied: NOT CONNECTED");
                            $.mobile.loading('hide');
                            clearTimeout(timer);
                            refreshAPlist();
                        };
                        setTimeout(5000);
                    }, "text");
                    if (new Date().getTime() - startTime > 65000) {
                        $.mobile.loading('hide');
                        alert("TIMEOUT: NO RESPONSE FROM ESP");
                        clearInterval(timer);
                        return;
                    };
                }, 5000);
            };
        }, "text");
        $("#pass-1").val(""); // clear the password field after submit
    }



    function WiFiMoreinfo() {
        radioanswer = $('.wifiradio:checked').val();
        $.each(staticwifi, function(i, object) {
            if (object.ssid === radioanswer) {
                $("#wifiinsert").empty();
                $("#wifiinsert").append(
                    "<h3 class=\"centerwrapper\">" + radioanswer + "</h3>" +
                    "<div class=\"popup\">" +
                    "<br>Connected: " + object.connected +
                    "<br>RSSI: " + object.rssi +
                    "<br>Channel: " + object.channel +
                    "<br>Security: " + object.encyrpted +
                    "<br>BSSID: " + object.BSSID +
                    "</div>"
                );
                $("#wifiinsert").popup("open");
            };
        });
    };

    function stationboxes() {

        if ($("#flip-STA").val() == "on") {
            $("#flip-dhcp").flipswitch('enable');
            if ($("#flip-dhcp").val() == "on") {
                $("#STAform :text").textinput('disable');
            } else {
                $("#STAform :text").textinput('enable');
            }
        } else {
            $("#flip-dhcp").flipswitch('disable');
            $("#STAform :text").textinput('disable');
        }

    }



}); // end of wifipage create.. 

$(document).on("pagecreate", "#aboutpage", function() {
    //<!-- About page -->
    //var results; 
    //var staticwifi; 
    function GetAboutVars() {
        $.getJSON("data.esp?plain=AboutPage", function(results) {
            $("#aboutvars").empty();
            $("#aboutvars").append("<br>Version = " + results.version_var);
            $("#aboutvars").append("<br>Compile Date = " + results.compiletime_var);
            $("#aboutvars").append("<br>SDK Version = " + results.sdk_var);
            $("#aboutvars").append("<br>Heap = " + results.heap_var);
            $("#aboutvars").append("<br>Flash Size = " + results.flashsize_var);
            $("#aboutvars").append("<br>Chip ID = " + results.chipid_var);
            $("#aboutvars").append("<br>Flash ID = " + results.flashid_var);
            $("#aboutvars").append("<br>Sketch Size = " + results.sketchsize_var);
            $("#aboutvars").append("<br>Free Space = " + results.freespace_var);
            $("#aboutvars").append("<br>Millis = " + results.millis_var);
            $("#aboutvars").append("<br>UpTime = " + results.uptime_var);
            $("#aboutvars").append("<br>VCC = " + results.vcc_var);
            $("#aboutvars").append("<br>RSSI = " + results.rssi_var);
            $("#aboutvars").append("<br> CPU Speed = " + results.cpu_var + "Mhz");
        });
    };

    //  $(document).ready(function(){
    $(document).on('pageshow', '#aboutpage', function() {
        GetAboutVars();
    });

    $("#aboutvars").click(function() {
        GetAboutVars();
    });


    //  });

});