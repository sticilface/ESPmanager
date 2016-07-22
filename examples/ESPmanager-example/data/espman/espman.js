/****************************************************
 *                    Events
 *
 ****************************************************/
var _waiting_reboot = false;
var _upgrade_failed;

var source;

function startEvents() {
    if (!!window.EventSource) {

      if (source == null || source.readyState == 2) {
        source = new EventSource('events');
         var abc;

        source.onopen = function(e) {
            console.log("Events Opened", e);
            addMessage("Connected");
            if (_waiting_reboot) {
                $("#upgrade_message").empty().append("<h4>Done</h4>");
                clearTimeout(_upgrade_failed);
                _waiting_reboot = false;
                setTimeout(function() {
                    $("#upgradepopup").popup("close");
                }, 2000);

            }
        };

        source.onerror = function(e) {
            console.log("Events ERROR", e);
            if (e.target.readyState != EventSource.OPEN) {
                console.log("Disconnected");
                addMessage("Disconnected");
            }
        };

        source.addEventListener('message', function(e) {
            console.log("message", e.data);
            popUpMessage(e.data);
        }, false);

        source.addEventListener('console', function(e) {
            //console.log("message", e.data);
            addMessage(e.data);
        }, false);

        // source.addEventListener('myevent', function(e) {
        //   console.log("myevent", e.data);
        // }, false);
        //  upgrade is via JSON package
        source.addEventListener('upgrade', function(e) {
            //console.log("upgrade event", e.data);
            if (e.data == "begin") {
                $("#upgrade_message").empty();
                $("#updatebanner").empty().append("Upgrade Started");
                $("#upgrade-slider").val(0);
                $("#upgradepopup").popup("open");
                addMessage("Upgrade Started");
            } else if (e.data == "end") {
                addMessage("Upgrade Finished");
                setTimeout(function() {
                    $("#upgradepopup").popup("close");
                }, 1000);
            } else if ($.isNumeric(e.data) && e.data >= 0 && e.data <= 100) {
                //console.log(e.data);
                $("#upgrade-slider").val(e.data);
                $("#upgradepopup").popup("open");
                $("#upgradepopup").enhanceWithin().popup();
            } else if (e.data == "firmware") {
                $("#upgrade_message").empty().append("<h4>Updating Firmware...</h4>");
                _upgrade_failed = setTimeout(function() {
                    $("#upgrade_message").empty().append("<h4>REBOOT failed: Device Not online</h4>");
                    setTimeout(function() {
                        $("#upgradepopup").popup("close");
                        location.reload();
                    }, 1000);
                }, 90000);
            } else if (e.data == "firmware-end") {
                $("#upgrade_message").empty().append("<h4>Rebooting...</h4>");
                _waiting_reboot = true;
                clearTimeout(_upgrade_failed);
                _upgrade_failed = setTimeout(function() {
                    $("#upgrade_message").empty().append("<h4>REBOOT failed: Device Not online</h4>");
                    setTimeout(function() {
                        $("#upgradepopup").popup("close");
                        location.reload();
                    }, 1000);
                }, 60000);

            } else {
                $("#upgrade_message").empty().append("<h4>" + e.data + "</h4>");
                $("#upgradepopup").enhanceWithin().popup();
                $('#upgradepopup').popup({ dismissible: true });

                //$('#upgradepopup').popup('open', { dismissible: true });
                addMessage(e.data);

            }
        });

        // OTA update
        source.addEventListener('update', function(e) {

            if (e.data == "begin") {
                $("#upgrade_message").empty();
                $("#updatebanner").empty().append("OTA Update Started");
                $("#upgrade-slider").val(0);
                $("#upgradepopup").popup("open");
                addMessage("OTA upgrade Started");
            } else if (e.data == "end") {
                addMessage("OTA Finished");
                $("#upgrade_message").empty().append("<h4>Rebooting...</h4>");
                _waiting_reboot = true;
                _upgrade_failed = setTimeout(function() {
                    $("#upgrade_message").empty().append("<h4>REBOOT failed: Device Not online</h4>");
                    setTimeout(function() {
                        $("#upgradepopup").popup("close");
                    }, 2000);
                }, 60000);
            } else if ($.isNumeric(e.data) && e.data >= 0 && e.data <= 100) {
                $("#upgrade-slider").val(e.data);
                $("#upgradepopup").enhanceWithin().popup();
            } else {
                $("#upgrade_message").empty().append("<h4>" + e.data + "</h4>");
                $("#upgradepopup").enhanceWithin().popup();
                addMessage(e.data);
            }

        }, false);

    }
    }
}

// function isNumeric(n) {
//   return !isNaN(parseFloat(n)) && isFinite(n);
// }



/****************************************************
 *                    WebSocket
 *
 ****************************************************/


var ws = null;
var _messageCount = 0;

function ge(s) {
    return document.getElementById(s);
}

function ce(s) {
    return document.createElement(s);
}

function addMessage(m) {
    if (_messageCount > 14) {
        $("#wsmessages").find('div').first().remove();
        _messageCount = _messageCount - 1;
    }
    _messageCount = _messageCount + 1;
    var msg = ce("div");
    msg.innerText = m;
    $("#wsmessages").append(msg);

}

function popUpMessage(m) {
    addMessage(m);
    var msg = ce("div");
    msg.innerText = m;
    $("#wsalert").append(msg);
    $("#wsalert").popup("open");

    setTimeout(function() {
        $("#wsalert").popup("close");

    }, 6000);
}

// function startSocket() {
//     ws = new ReconnectingWebSocket('ws://' + document.location.host + '/espman/ws', ['arduino']);
//     ws.binaryType = "arraybuffer";
//     ws.onopen = function(e) {
//         addMessage("WebSocket Connected");
//         popUpMessage("WebSocket Connected");
//     };
//     ws.onclose = function(e) {
//         addMessage("WebSocket Disconnected");
//         popUpMessage("WebSocket Disconnected");
//         // setTimeout(function() {
//         //     startSocket()
//         // }, 5000);
//     };
//     ws.onerror = function(e) {
//         //console.log("ws error", e);
//         addMessage("WebSocket Error");
//         popUpMessage("WebSocket Error");
//     };
//     ws.onmessage = function(e) {
//       addMessage(e.data);
//       popUpMessage(e.data);

//     };

//     // setInterval(function() {
//     //     console.log("WS REFRESH");
//     //     ws.refresh();
//     // }, 10000);

// }


$("#wsalert").on("popupafterclose", function(event, ui) {

    $("#wsalert").empty();

});



/****************************************************
 *                    POP up creates
 *
 ****************************************************/
$("#upgradepopup").on("popupcreate", function(event, ui) {
    console.log("upgradepopup called");
    $("input").remove();
    $(".ui-slider-handle").remove();
    $('.ui-slider-track').css('margin', '0 15px 0 15px').css('pointer-events', 'none');


});

//$( ".selector" ).on( "popupcreate", function( event, ui ) {} );



/****************************************************
 *                   Global funtions
 *
 ****************************************************/
$(function() {

    $("[data-role='header']").toolbar();
    $("[data-role='footer']").toolbar();
    $("body>[data-role='panel']").panel();

    $('#savebutton').hide();
});

function getGenvars() {

    $.post("data.esp", "WiFiDetails", function(result) {

      console.log(result);

      datatosave(result);

      if (result.hasOwnProperty("STA")) {

      if (result.STA.hasOwnProperty("state")) {
        if (result.STA.state) {
            $("#gen-page-status").empty().append(" <p> Connected to " + result.STA.connectedssid + " (<a href=\"http://"+ result.STA.IP + " \" rel=\"external\", data-ajax=\"false\">" + result.STA.IP + "</a>)</p>");
        } else {
           $("#gen-page-status").empty().append(" <p> WiFi Not Connected to " + result.STA.connectedssid + "</p>");
        }
      }

      }

    if (result.hasOwnProperty("General")) {

      if (result.General.hasOwnProperty("deviceid")) {
       $("#device-name").val(result.General.deviceid);
      }

      if (result.General.hasOwnProperty("ap_boot_mode")) {
        console.log("ap_boot_mode: "+ result.General.ap_boot_mode);
        $("#select-AP-behav").val(result.General.ap_boot_mode).selectmenu("refresh");
      }

      if (result.General.hasOwnProperty("no_sta_mode")) {
        console.log("no_sta_mode: "+ result.General.no_sta_mode);
        $("#select-STA-behav").val(result.General.no_sta_mode).selectmenu("refresh");
      }

      if (result.General.hasOwnProperty("mDNS")) {
        $("#flip-mdnsenable").val((result.General.mDNS) ? "on" : "off").flipswitch('refresh');
      }

      if (result.General.hasOwnProperty("usePerminantSettings")) {
        $("#flip-usePerminantSettings").val((result.General.usePerminantSettings) ? "on" : "off").flipswitch('refresh');
      }

      }

    });
} // end of getgenvars func


/****************************************************
 *                    Panel Create
 *
 ****************************************************/
$(document).on("panelcreate", function(event, ui) {

    // $("#rebootbutton").click(function() {
    //   console.log("REBOOT PRESSED");
    //     $.post("data.esp", "reboot");
    // });

    // $("#upgradebutton").click(function() {
    //    $.post("data.esp", "upgrade");
    // })


});

// $(document).off('click', '.upgradebutton').on('click', '.upgradebutton', function(e) {
//     $.post("data.esp", "upgrade");
// });

$(document).off('click', '#rebootbutton').on('click', '#rebootbutton', function(e) {
    $.post("data.esp", "reboot");
});

$(document).on('vclick', '.upgradebutton', function(e) {
    $.post("data.esp", "PerformUpdate=true");
});

$(document).on('touchstart click', '.myheader', function(e) {
    //console.log("#myheader click");
    e.preventDefault();
    // $.post("data.esp", $.mobile.activePage.attr('id'), function(result) {
    //   datatosave(result);
    // });

    var currentpage = $.mobile.activePage.attr('id');

    if (currentpage == "generalpage") {
      getGenvars();
    } else if (currentpage == "wifipage") {
      getWiFiVars(false);
    } else if (currentpage == "aboutpage") {
      GetAboutVars();
    } else if (currentpage == "appage") {
      getAPvars();
    } else if (currentpage == "otapage") {
      getOTAvars();
    } else if (currentpage == "upgradepage"){
      getUpgradeVars();
    }
    //startEvents();
    //console.log(source);


    //ws.send($.mobile.activePage.attr('id'));
});

/****************************************************
 *                   General Page
 *
 ****************************************************/

$(document).on("pageshow", "#generalpage", function() {


    getGenvars();



}); // end of general page..

$(document).on("pagecreate", "#generalpage", function() {

    // console.log("page create general");

    //     $("#general-1-submit").click(function() {
    //       console.log("UFCK ASS");
    //       $.post("data.esp", $(this.form).serialize());
    //   });

});

$("#general-1-submit").bind("click", function(event, ui) {
    $.post("data.esp", $(this.form).serialize(), function(e) {
      datatosave(e);
    });
});

$(document).off('click', '#savebutton').on('click', '#savebutton', function(e) {
    $.post("data.esp", "save", function(e) {
      datatosave(e);
    });
});

/****************************************************
 *                    OTA Page
 *
 ****************************************************/
$(document).on("pagecreate", "#otapage", function() {

    $('#updaterform').fileUpload({
        complete: function(jqXHR, textStatus) {
                console.log(jqXHR);
            } // Callback on completion
    });


    // $.getJSON("data.esp?plain=WiFiDetails", function(result) {

    //     //$("#device-name").val("").attr("placeholder", result.general.deviceid).blur();

    //         if (result.general.OTAusechipID === true) {
    //             $("#flip-OTAusechipID").val('Yes').flipswitch('refresh');
    //         } else {
    //             $("#flip-OTAusechipID").val('No').flipswitch('refresh');
    //         }


    //         if (result.general.OTAenabled === true) {
    //             $("#flip-otaenable").val('on').flipswitch('refresh');
    //         } else {
    //             $("#flip-otaenable").val('off').flipswitch('refresh');
    //         }

    //         });


    // $("#flip-otaenable").change( function() {
    //     $.post("data.esp", $(this).serialize());
    // })
    // $("#flip-OTAusechipID").change( function() {
    //     $.post("data.esp", $(this).serialize());
    // })



});

/****************************************************
 *                    WiFi Page
 *
 ****************************************************/
$(document).on("pagecreate", "#wifipage", function() {

    // Variables
    var staticwifi;
    var globalwifi;

    getWiFiVars(false);

    setTimeout(function() {
        getWiFiVars(true);
    }, 1000);

    $("#flip-dhcp").change(function() {
        stationboxes();
    });
    $("#flip-STA").change(function() {
        stationboxes();
    });

    $("#apply_sta").click(function() {
        $.post("data.esp", $(this.form).serialize());
        $("#stacollapse").collapsible("collapse");
        setTimeout(function() {
            getWiFiVars(false);
        }, 2000);

    });

    $("#STA_settings_div").click(function() {
        getWiFiVars(false);
    });

    $("#ssid-1-rescan").click(function() {
        getWiFiVars(true);
    });

    $("#ssid-1-submit").click(function() {
        submitnewssid();
    });
    // $("#rebootbutton").click(function() {
    //     $.post("data.esp", "reboot");
    // });
    // $("#resetwifi").click(function() {
    //     $.post("data.esp", "resetwifi");
    // });
    $("#refreshdata").click(function() {
        getWiFiVars(false);
    });

    //$( document ).delegate("#wifipage", "pageinit", function() {
    //alert('A page with an id of "aboutPage" was just created by jQuery Mobile!');

    $("#general-1-submit").click(function() { // this is being used by the front page
        $.post("data.esp", $(this.form).serialize(), function(e) {
          console.log(e);
            datatosave(e);
        }).success(function(e) {
            //$("#status").empty().append("Connected").css("color", "green");
            console.log(e);
            datatosave(e);
        });
        var val = $("#device-name").val();
        $("#device-name").val("").attr("placeholder", val).blur();
        //return false;
    });

    // $(".sendsubmit").click(function() {
    //     $.post("data.esp", $(this.form).serialize());
    //     $('[data-role="popup"]').popup("close");
    //     setTimeout(function() {
    //         getWiFiVars(false);
    //     }, 5000);
    // });

    $('form').submit(function() {
        //if( $(this).id == "updater-button") { alert("yes"); return true;}
        return false;
    });


    function refreshAPlist() {
        $.post("data.esp", "PerformWiFiScan", function(result) {
          datatosave(result);
          if (result.hasOwnProperty("networks")) {
                globalwifi = result.networks;
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
          }
            }).success(function() {
                //$("#status").empty().append("Connected").css("color", "green");
            })
            .error(function() {
                //$("#status").empty().append("Not Connected").css("color", "red");
            })
            .complete(function() {});
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
                        }
                        setTimeout(5000);
                    }, "text");
                    if (new Date().getTime() - startTime > 65000) {
                        $.mobile.loading('hide');
                        alert("TIMEOUT: NO RESPONSE FROM ESP");
                        clearInterval(timer);
                        return;
                    }
                }, 5000);
            }
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

    function stationboxes() {

        if ($("#flip-STA").val() == "on") {
            $("#flip-dhcp").flipswitch('enable');
            if ($("#flip-dhcp").val() == "on") {
                $("#STAform :text").not("#text-STA-set-mac").textinput('disable');
            } else {
                $("#STAform :text").textinput('enable');
            }
        } else {
            $("#flip-dhcp").flipswitch('disable');
            $("#STAform :text").not("#text-STA-set-mac").textinput('disable');
        }

    }

}); // end of wifipage create..


    function getWiFiVars(scan) {

        var request;
        if (scan) {
            request = "PerformWiFiScan";
        }
        if (!scan) {
            request = "WiFiDetails";
        }
        $.post("data.esp", request, function(result) {

              globalwifi = result;

              datatosave(result);


                if ("networks" in result) {
                    staticwifi = result.networks;
                    $("#wifinetworks-data").empty();
                    $("#wifinetworks-data").append("<legend>Select WiFi Network:</legend>");
                    $.each(result.networks, function(i, object) {
                        var isconnected = " ";
                        if (object.connected === true) isconnected = "checked=\"checked\"";
                        $("#wifinetworks-data").append("<input class = \"wifiradio\" type=\"radio\" name=\"ssid\" id=\"radio-choice-v-" + i + "a\" value=\"" +
                            object.ssid + "\"" + isconnected + "><label for=\"radio-choice-v-" + i + "a\">" + object.ssid + "</label>");
                    });
                    $("#wifinetworks-data").enhanceWithin();
                }
                //};  / end of if for data test
                if (result.hasOwnProperty("STA")) {
                    if (result.STA.hasOwnProperty("dhcp")) {
                        if (result.STA.dhcp === true) {
                            $('#flip-dhcp').val('on').flipswitch('refresh');
                            $("#STA_dhcp").empty().append("DHCP: Enabled");
                        }
                        if (result.STA.dhcp === false) {
                            $('#flip-dhcp').val('off').flipswitch('refresh');
                            $("#STA_dhcp").empty().append("DHCP: Disabled");
                        }
                    }
                    if (result.STA.hasOwnProperty("state")) {
                        if (result.STA.state === true) {
                            $("#flip-STA").val('on').flipswitch('refresh');
                        }
                        if (result.STA.state === false) {
                            $("#flip-STA").val('off').flipswitch('refresh');
                        }

                        $("#STA_state").empty().append((result.STA.state) ? "State: Enabled" : "State: Disabled");

                        if (result.STA.state) {
                            $("#STA_connectedto").empty().append("Connected to: " + result.STA.connectedssid);
                        }
                    }




                $("#STA_ip").empty().append("IP: " + result.STA.IP);
                $("#text-STA-set-ip").val(result.STA.IP);

                $("#STA_channel").empty().append("Channel: " + result.STA.channel);

                $("#STA_gateway").empty().append("Gateway: " + result.STA.GW);
                $("#text-STA-set-gw").val(result.STA.GW);

                $("#STA_subnet").empty().append("Subnet: " + result.STA.SN);
                $("#text-STA-set-sn").val(result.STA.SN);

                $("#STA_dns1").empty().append("DNS1: " + result.STA.DNS1);
                $("#text-STA-set-DNS1").val(result.STA.DNS1);

                $("#STA_dns2").empty().append("DNS2: " + result.STA.DNS2);
                $("#text-STA-set-DNS2").val(result.STA.DNS2);

                $("#STA_mac").empty().append("MAC: " + result.STA.MAC);
                $("#text-STA-set-mac").val(result.STA.MAC);

                }
                // DNS.. no functions in espWiFi lib to store it yet...
                // $("#STA_dns").empty().append("DNS: " + result.DNS.subnet);
                // $("#text-DNS-set-sn").val(result.DNS.subnet);

                if ($("#flip-dhcp").val() == "on") {
                    $("#STAform :text").not("#text-STA-set-mac").textinput('disable');
                }
                if ($("#flip-dhcp").val() == "off") {
                    $("#STAform :text").textinput('enable');
                }
                if ($("flip-STA").val() == "on") {
                    $("#flip-dhcp").flipswitch('enable');
                }
                if ($("flip-STA").val() == "off") {
                    $("#flip-dhcp").flipswitch('disable');
                } //.slider('disable');

                $("#wifipage").enhanceWithin();

            }).success(function() {
                $("#status").empty().append("Connected").css("color", "green");
            })
            .error(function() {
                $("#status").empty().append("Not Connected").css("color", "red");
            })
            .complete(function() {});
    }


/****************************************************
 *                    About Page
 *
 ****************************************************/
    //<!-- About page -->
    //var results;
    //var staticwifi;
    function GetAboutVars() {
        $.post("data.esp", "AboutPage", function(results) {
          datatosave(results);

                $("#aboutvars").empty();
                $("#aboutvars").append("<br>Version = " + results.version_var);
                $("#aboutvars").append("<br>Compile Date = " + results.compiletime_var);

                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>Heap = " + results.heap_var);
                $("#aboutvars").append("<br>Millis = " + results.millis_var);
                $("#aboutvars").append("<br>UpTime = " + results.uptime_var);

                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>Chip ID = " + results.chipid_var);
                $("#aboutvars").append("<br>Core Version = " + results.core_var);
                $("#aboutvars").append("<br>SDK Version = " + results.sdk_var);
                $("#aboutvars").append("<br>Boot Version = " + results.bootverion_var);
                $("#aboutvars").append("<br>Boot Mode = " + results.bootmode_var);
                $("#aboutvars").append("<br> CPU Speed = " + results.cpu_var + "Mhz");
                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>SPIFFS Size = " + results.SPIFFS.totalBytes);
                $("#aboutvars").append("<br>SPIFFS Used = " + results.SPIFFS.usedBytes);
                $("#aboutvars").append("<br>SPIFFS Blocksize = " + results.SPIFFS.blockSize);
                $("#aboutvars").append("<br>SPIFFS Pagesize = " + results.SPIFFS.pageSize);
                $("#aboutvars").append("<br>SPIFFS Max Open Files = " + results.SPIFFS.maxOpenFiles);
                $("#aboutvars").append("<br>SPIFFS Max Path Length = " + results.SPIFFS.maxPathLength);

                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>Flash ID = " + results.flashid_var);
                $("#aboutvars").append("<br>Flash Size = " + results.flashsize_var);
                $("#aboutvars").append("<br>Flash Real Size = " + results.flashRealSize_var);
                $("#aboutvars").append("<br>Flash Size by ID = " + results.flashchipsizebyid_var);
                $("#aboutvars").append("<br>Flash Chip Mode = " + results.flashchipmode_var);
                $("#aboutvars").append("<br>");

                //  UMN MALLOC
                // UMMobject[F("totalEntries")] = ummHeapInfo.totalEntries;
                // UMMobject[F("usedEntries")] = ummHeapInfo.usedEntries;
                // UMMobject[F("freeEntries")] = ummHeapInfo.freeEntries;
                // UMMobject[F("totalBlocks")] = ummHeapInfo.totalBlocks;
                // UMMobject[F("usedBlocks")] = ummHeapInfo.usedBlocks;
                // UMMobject[F("freeBlocks")] = ummHeapInfo.freeBlocks;
                // UMMobject[F("maxFreeContiguousBlocks")] = ummHeapInfo.maxFreeContiguousBlocks;

                $("#aboutvars").append("<br>UMM HEAP INFO ");
                $("#aboutvars").append("<br>Total Entries = " + results.UMM.totalEntries);
                $("#aboutvars").append("<br>Used Entries = " + results.UMM.usedEntries);
                $("#aboutvars").append("<br>Free Entries = " + results.UMM.freeEntries);
                $("#aboutvars").append("<br>Total Blocks = " + results.UMM.totalBlocks);
                $("#aboutvars").append("<br>Used Blocks = " + results.UMM.usedBlocks);
                $("#aboutvars").append("<br>Free Blocks = " + results.UMM.freeBlocks);
                $("#aboutvars").append("<br>Max Free Contiguous Blocks = " + results.UMM.maxFreeContiguousBlocks);

                $("#aboutvars").append("<br>");
                $("#aboutvars").append("<br>Sketch Size = " + results.sketchsize_var);
                $("#aboutvars").append("<br>Free Space = " + results.freespace_var);
                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>VCC = " + results.vcc_var);
                $("#aboutvars").append("<br>RSSI = " + results.rssi_var);
                $("#aboutvars").append("<br>");

                $("#aboutvars").append("<br>Reset Reason = " + results.reset.resaon);
                $("#aboutvars").append("<br>Reset info = " + results.reset.info);

            }).success(function() {
                //$("#status").empty().append("Connected").css("color", "green");
            })
            .error(function() {
                //$("#status").empty().append("Not Connected").css("color", "red");
            })
            .complete(function() {});
    }

$(document).on("pagecreate", "#aboutpage", function() {


    //  $(document).ready(function(){
    $(document).on('pageshow', '#aboutpage', function() {
        GetAboutVars();
    });

    $("#aboutvars").click(function() {
        GetAboutVars();
    });


    //  });

});

/****************************************************
 *                    AP Page
 *
 ****************************************************/

     function getAPvars() {

        $.post("data.esp", "WiFiDetails", function(result) {

                datatosave(result);

        if (result.hasOwnProperty("AP")) {

                if (result.AP.state === true) {
                    $('#flip-AP').val('on').flipswitch('refresh');
                }
                if (result.AP.state === false) {
                    $('#flip-AP').val('off').flipswitch('refresh');
                }

                if (result.AP.hidden === true) {
                    $('#flip-AP-hidden').val('on').flipswitch('refresh');
                }
                if (result.AP.hidden === false) {
                    $('#flip-AP-hidden').val('off').flipswitch('refresh');
                }

                $("#AP_hidden").empty().append((result.AP.visible) ? "Hidden: No" : "Hidden: Yes");

                $("#STA_mac").empty().append("MAC: " + result.STA.MAC);
                $("#text-STA-set-mac").val(result.STA.MAC);

                $("#AP_state").empty().append((result.AP.state) ? "State: Enabled" : "State: Disabled");

                $("#AP_ssid").empty().append("SSID: " + result.AP.ssid);
                // $("#text-AP-set-ssid").val(result.AP.ssid);

                $("#AP_pass").empty().append("Password: " + result.AP.pass);
                $("#text-AP-set-pass").val(result.AP.pass);

                $("#AP_ip").empty().append("IP: " + result.AP.IP);
                $("#text-AP-set-ip").val(result.AP.IP);

                $("#AP_channel").empty().append("Channel: " + result.AP.channel);
                $("#text-AP-set-channel").val(result.AP.channel);

                $("#AP_mac").empty().append("MAC: " + result.AP.MAC);
                //$("#text-AP-set-mac").val(result.AP.MAC);
        }
                //  Update slider elements etc..

                if ($("#flip-AP").val() == "on") {
                    $("#APform :text").textinput('enable');
                    $("#flip-AP-hidden").flipswitch('enable');
                }
                if ($("#flip-AP").val() == "off") {
                    $("#APform :text").textinput('disable');
                    $("#flip-AP-hidden").flipswitch('disable');
                }

            }).success(function(e) {
            //$("#status").empty().append("Connected").css("color", "green");
            datatosave(e);
        }).error(function() {
                //$("#status").empty().append("Not Connected").css("color", "red");
            })
            .complete(function() {});

    }


$(document).on("pagecreate", "#appage", function() {
    //<!-- About page -->

    getAPvars();

    $("#flip-AP").change(function() {
        if ($(this).val() == "on") {
            $("#APform :text").textinput('enable');
            $("#flip-AP-hidden").flipswitch('enable');
        }
        if ($(this).val() == "off") {
            $("#APform :text").textinput('disable');
            $("#flip-AP-hidden").flipswitch('disable');
        }
    });

    $("#AP_settings_div").click(function() {
        getAPvars();
    });




    $("#apply_ap").click(function() {
        $.post("data.esp", $(this.form).serialize()).success(function(e) {
            //$("#status").empty().append("Connected").css("color", "green");
            datatosave(e);
        });
        $("#apcollapse").collapsible("collapse");
        setTimeout(function() {
            getAPvars();
        }, 2000);

    })

});

/****************************************************
 *                    Upgrade Page
 *
 ****************************************************/

 function getUpgradeVars() {
   $.post("data.esp", "generalpage", function(result) {
           // Repo : <var id="field-repo"></var> <br>
           // Branch : <var id="field-branch"></var> <br>
           // Commit : <var id="field-commit"></var> <br>
           // id="text-UpgradeURL"
           // id="text-Upgrade-freq"

           console.log(result);

          datatosave(result);

       if (result.hasOwnProperty("General")) {


           // $("#field-repo").empty().append(result.REPO);
           // $("#field-branch").empty().append(result.BRANCH);
           // $("#field-commit").empty().append(result.COMMIT);



           if (result.General.hasOwnProperty("updateURL")) {
             $("#text-UpgradeURL").val(result.General.updateURL);
           }
           if (result.General.hasOwnProperty("updateFreq")) {
             $("#text-Upgrade-freq").val(result.General.updateFreq);
           } else {
             $("#text-Upgrade-freq").val("0");
           }




         }


       }).success(function(e) {
           //$("#status").empty().append("Connected").css("color", "green");
           datatosave(e);
       })
       .error(function() {
           //$("#status").empty().append("Not Connected").css("color", "red");
       })
       .complete(function() {});

 }


$(document).on("pageshow", "#upgradepage", function() {

  getUpgradeVars();




    $("#updatesubmitbutton").click(function() {
        $.post("data.esp", $(this).closest("form").find('input,select').filter(':visible').serialize(), function(data) {
            //console.log("Data Sent");
        }).success(function(e) {
          datatosave(e);
        });
    });

    // $("#checkforupdatebutton").click(function() {

    //     $.post("data.esp", "PerformUpdate=true");

    // });

});

/****************************************************
 *                    OTA Page
 *
 ****************************************************/
 $(document).on("pageshow", "#otapage", function() {

   getOTAvars();



 });


 function getOTAvars() {
   console.log("getOTAvars");

   $.post("data.esp", "generalpage", function(result) {

      datatosave(result);

if (result.hasOwnProperty("General")) {

        if (result.General.hasOwnProperty("OTAupload")) {

            if (result.General.OTAupload === true) {
               $('#flip-otaenable').val('on').flipswitch('refresh');
            } else {
              $('#flip-otaenable').val('off').flipswitch('refresh');
            }
        }

        if (result.General.hasOwnProperty("OTAport")) {

          $("#text-OTA-set-port").val(result.General.OTAport);

        }

        if (result.General.hasOwnProperty("OTApassword")) {

          //$("#text-OTA-set-pass").val(result.General.OTApassword);
          if (result.General.OTApassword) {
            $("#hasOTApassword").empty().append("OTA Password Set");
          } else {
            $("#hasOTApassword").empty().append("NO OTA Password Set");
          }

        }

}

   });


 }


 $("#apply_ota").bind("click", function(event, ui) {
    $.post("data.esp", $(this.form).serialize(), function(e) {
      datatosave(e);
      getOTAvars();
    });
});


//}); // page init

function datatosave(json) {
  if (json.hasOwnProperty("changed")) {
    if (json.changed === true) {
      $('#savebutton').show();
    } else {
      $('#savebutton').hide();
    }
  }
}

/**
 * fileUpload
 * http://abandon.ie
 *
 * Plugin to add file uploads to jQuery ajax form submit
 *
 * November 2013
 *
 * @version 0.9
 * @author Abban Dunne http://abandon.ie
 * @license MIT
 *
 */
;
(function($, window, document, undefined) {
    // Create the defaults once
    var pluginName = "fileUpload",
        defaults = {
            uploadData: {},
            submitData: {},
            uploadOptions: {},
            submitOptions: {},
            before: function() {},
            beforeSubmit: function() {
                return true;
            },
            success: function() {},
            error: function() {},
            complete: function() {}
        };

    // The actual plugin constructor
    function Plugin(element, options) {
        this.element = element;
        this.$form = $(element);
        this.$uploaders = $('input[type=file]', this.element);
        this.files = {};
        this.settings = $.extend({}, defaults, options);
        this._defaults = defaults;
        this._name = pluginName;
        this.init();
    }

    Plugin.prototype = {
        /**
         * Initialize the plugin
         *
         * @return void
         */
        init: function() {
            this.$uploaders.on('change', {
                context: this
            }, this.processFiles);
            this.$form.on('submit', {
                context: this
            }, this.uploadFiles);
        },



        /**
         * Process files after they are added
         *
         * @param  jQuery event
         * @return void
         */
        processFiles: function(event) {
            var self = event.data.context;
            self.files[$(event.target).attr('name')] = event.target.files;
        },



        /**
         * Handles the file uploads
         *
         * @param  jQuery event
         * @return void
         */
        uploadFiles: function(event) {
            event.stopPropagation(); // Stop stuff happening
            event.preventDefault(); // Totally stop stuff happening

            var self = event.data.context;

            // Run the before callback
            self.settings.before();

            // Declare a form data object
            var data = new FormData();
            data.append('file_upload_incoming', '1');

            // Add the files
            $.each(self.files, function(key, field) {
                $.each(field, function(key, value) {
                    data.append(key, value);
                });
            });

            // If there is uploadData passed append it
            $.each(self.settings.uploadData, function(key, value) {
                data.append(key, value);
            });

            // Perform Ajax call
            $.ajax($.extend({}, {
                url: self.$form.attr('action'),
                type: 'POST',
                data: data,
                cache: false,
                dataType: 'json',
                processData: false, // Don't process the files, we're using FormData
                contentType: false, // Set content type to false as jQuery will tell the server its a query string request
                success: function(data, textStatus, jqXHR) {
                    self.processSubmit(event, data);
                },
                error: function(jqXHR, textStatus, errorThrown) {
                    self.settings.error(jqXHR, textStatus, errorThrown);
                }
            }, self.settings.uploadOptions));
        },



        /**
         * Submits form data with files
         *
         * @param  jQuery event
         * @param  object
         * @return void
         */
        processSubmit: function(event, uploadData) {
            var self = event.data.context;

            // Run the beforeSubmit callback
            if (!self.settings.beforeSubmit(uploadData)) return;

            // Serialize the form data
            var data = self.$form.serializeArray();

            // Loop through the returned array from the server and add it to the new POST
            $.each(uploadData, function(key, value) {
                data.push({
                    'name': key,
                    'value': value
                });
            });

            // If there is uploadData passed append it
            $.each(self.settings.submitData, function(key, value) {
                data.push({
                    'name': key,
                    'value': value
                });
            });

            $.ajax($.extend({}, {
                url: self.$form.attr('action'),
                type: 'POST',
                data: data,
                cache: false,
                dataType: 'json',
                success: function(data, textStatus, jqXHR) {
                    self.settings.success(data, textStatus, jqXHR);
                },
                error: function(jqXHR, textStatus, errorThrown) {
                    self.settings.error(jqXHR, textStatus, errorThrown);
                },
                complete: function(jqXHR, textStatus) {
                    self.settings.complete(jqXHR, textStatus);
                }
            }, self.settings.submitOptions));
        }
    };

    $.fn[pluginName] = function(options) {
        return this.each(function() {
            if (!$.data(this, "plugin_" + pluginName)) {
                $.data(this, "plugin_" + pluginName, new Plugin(this, options));
            }
        });
    };

})(jQuery, window, document);

//https://github.com/joewalnes/reconnecting-websocket/blob/master/reconnecting-websocket.min.js

! function(a, b) {
    "function" == typeof define && define.amd ? define([], b) : "undefined" != typeof module && module.exports ? module.exports = b() : a.ReconnectingWebSocket = b()
}(this, function() {
    function a(b, c, d) {
        function l(a, b) {
            var c = document.createEvent("CustomEvent");
            return c.initCustomEvent(a, !1, !1, b), c
        }
        var e = {
            debug: !1,
            automaticOpen: !0,
            reconnectInterval: 1e3,
            maxReconnectInterval: 3e4,
            reconnectDecay: 1.5,
            timeoutInterval: 2e3
        };
        d || (d = {});
        for (var f in e) this[f] = "undefined" != typeof d[f] ? d[f] : e[f];
        this.url = b, this.reconnectAttempts = 0, this.readyState = WebSocket.CONNECTING, this.protocol = null;
        var h, g = this,
            i = !1,
            j = !1,
            k = document.createElement("div");
        k.addEventListener("open", function(a) {
            g.onopen(a)
        }), k.addEventListener("close", function(a) {
            g.onclose(a)
        }), k.addEventListener("connecting", function(a) {
            g.onconnecting(a)
        }), k.addEventListener("message", function(a) {
            g.onmessage(a)
        }), k.addEventListener("error", function(a) {
            g.onerror(a)
        }), this.addEventListener = k.addEventListener.bind(k), this.removeEventListener = k.removeEventListener.bind(k), this.dispatchEvent = k.dispatchEvent.bind(k), this.open = function(b) {
            h = new WebSocket(g.url, c || []), b || k.dispatchEvent(l("connecting")), (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "attempt-connect", g.url);
            var d = h,
                e = setTimeout(function() {
                    (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "connection-timeout", g.url), j = !0, d.close(), j = !1
                }, g.timeoutInterval);
            h.onopen = function() {
                clearTimeout(e), (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "onopen", g.url), g.protocol = h.protocol, g.readyState = WebSocket.OPEN, g.reconnectAttempts = 0;
                var d = l("open");
                d.isReconnect = b, b = !1, k.dispatchEvent(d)
            }, h.onclose = function(c) {
                if (clearTimeout(e), h = null, i) g.readyState = WebSocket.CLOSED, k.dispatchEvent(l("close"));
                else {
                    g.readyState = WebSocket.CONNECTING;
                    var d = l("connecting");
                    d.code = c.code, d.reason = c.reason, d.wasClean = c.wasClean, k.dispatchEvent(d), b || j || ((g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "onclose", g.url), k.dispatchEvent(l("close")));
                    var e = g.reconnectInterval * Math.pow(g.reconnectDecay, g.reconnectAttempts);
                    setTimeout(function() {
                        g.reconnectAttempts++, g.open(!0)
                    }, e > g.maxReconnectInterval ? g.maxReconnectInterval : e)
                }
            }, h.onmessage = function(b) {
                (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "onmessage", g.url, b.data);
                var c = l("message");
                c.data = b.data, k.dispatchEvent(c)
            }, h.onerror = function(b) {
                (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "onerror", g.url, b), k.dispatchEvent(l("error"))
            }
        }, 1 == this.automaticOpen && this.open(!1), this.send = function(b) {
            if (h) return (g.debug || a.debugAll) && console.debug("ReconnectingWebSocket", "send", g.url, b), h.send(b);
            throw "INVALID_STATE_ERR : Pausing to reconnect websocket"
        }, this.close = function(a, b) {
            "undefined" == typeof a && (a = 1e3), i = !0, h && h.close(a, b)
        }, this.refresh = function() {
            h && h.close()
        }
    }

    return a.prototype.onopen = function() {}, a.prototype.onclose = function() {}, a.prototype.onconnecting = function() {}, a.prototype.onmessage = function() {}, a.prototype.onerror = function() {}, a.debugAll = !1, a.CONNECTING = WebSocket.CONNECTING, a.OPEN = WebSocket.OPEN, a.CLOSING = WebSocket.CLOSING, a.CLOSED = WebSocket.CLOSED, a
});
