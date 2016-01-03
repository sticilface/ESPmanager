
/****************************************************
 *                    Variables
 * 
 ****************************************************/

//$(  globalwifi ); 
//$(document).on("pageinit", function() {

    // $( document ).on( "mobileinit", function() {


    // });


/****************************************************
 *                    POP up creates
 * 
 ****************************************************/
$( "popup" ).on( "popupcreate", function( event, ui ) {



} );

/****************************************************
 *                   Global funtions
 * 
 ****************************************************/
 
    function getGenvars() {
        $.getJSON("data.esp?plain=WiFiDetails", function(result) {
          
            $("#select-AP-behav").val(result.general.APrestartmode).selectmenu( "refresh" );

              //  $("#select-AP-behav").attr("checked",true).checkboxradio("refresh");
            
            $("#flip-mdnsenable").val( (result.general.mDNSenabled)? "on":"off").flipswitch('refresh');
                
                
            console.log("Restart mode: " + result.general.APrestartmode ); 
            
            //$("#device-name").val("").attr("placeholder", result.general.deviceid );
            $("#device-name").val(result.general.deviceid);

            if (result.STA.state) {
                $("#gen-page-status").empty().append(" <p> Connected to " + result.STA.connectedssid + " (" + result.STA.IP + ")</p>");
            }

        }); 
    } // end of getgenvars func
    

/****************************************************
 *                    Panel Create 
 * 
 ****************************************************/
$( document ).on( "panelcreate", function( event, ui ) {

         $("#rebootbutton").click(function() {
            $.post("data.esp", "reboot");
         });

         // $("#upgradebutton").click(function() {
         //    $.post("data.esp", "upgrade"); 
         // })


});

$(document).off('click', '#upgradebutton').on('click', '#upgradebutton',function(e) {
    $.post("data.esp", "upgrade"); 
    }); 

/****************************************************
 *                   General Page
 * 
 ****************************************************/

$(document).on("pageshow", "#generalpage", function() {
  

      getGenvars();

    $("#general-1-submit").click(function() { 
        $.post("data.esp", $(this.form).serialize());
    });
    
}); // end of general page.. 

/****************************************************
 *                    OTA Page 
 * 
 ****************************************************/
$(document).on("pagecreate", "#otapage", function() {

        $('#updaterform').fileUpload({
                complete      : function(jqXHR, textStatus){ console.log(jqXHR); } // Callback on completion
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

    setTimeout( function() { getWiFiVars(true); }  , 1000);

    $("#flip-dhcp").change(function() {
        stationboxes();
    });
    $("#flip-STA").change(function() {
        stationboxes();
    });

    $("#apply_sta").click(function() {
        $.post("data.esp", $(this.form).serialize());
        $( "#stacollapse" ).collapsible( "collapse" );
            setTimeout(function() {
                getWiFiVars(false);
            } , 2000);

    })

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
        $.post("data.esp", $(this.form).serialize());
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
        $.getJSON("data.esp?plain=PerformWiFiScan", function(result) {
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
        }).success(function() { $("#status").empty().append("Connected").css("color", "green");  })
        .error(function() {   $("#status").empty().append("Not Connected").css("color", "red");  })
        .complete(function() {  });
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
                    "<br>Connected: " + ((object.connected)? "Yes" : "No") +
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
                $("#STAform :text").textinput('disable');
            } else {
                $("#STAform :text").textinput('enable');
            }
        } else {
            $("#flip-dhcp").flipswitch('disable');
            $("#STAform :text").textinput('disable');
        }

    }

 function getWiFiVars(scan) {
        
        var request;
        if (scan) {
            request = "data.esp?plain=PerformWiFiScan";
        }
        if (!scan) {
            request = "data.esp?plain=WiFiDetails";
        }
        $.getJSON(request, function(result) {
            globalwifi = result;


            
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

            if (result.STA.dhcp === true) {
                $('#flip-dhcp').val('on').flipswitch('refresh');
                $("#STA_dhcp").empty().append("DHCP: Enabled");

            }
            if (result.STA.dhcp === false) {
                $('#flip-dhcp').val('off').flipswitch('refresh');
                $("#STA_dhcp").empty().append("DHCP: Disabled");
            }

            if (result.STA.state === true) {
                $("#flip-STA").val('on').flipswitch('refresh');
            }
            if (result.STA.state === false) {
                $("#flip-STA").val('off').flipswitch('refresh');
            }

            $("#STA_state").empty().append((result.STA.state)? "State: Enabled":"State: Disabled");

            if (result.STA.state) {
                $("#STA_connectedto").empty().append("Connected to: "+result.STA.connectedssid);
            }

            $("#STA_ip").empty().append("IP: " + result.STA.IP);
            $("#text-STA-set-ip").val(result.STA.IP);
            $("#STA_gateway").empty().append("Gateway: " + result.STA.gateway);
            $("#text-STA-set-gw").val(result.STA.gateway);
            $("#STA_subnet").empty().append("Subnet: " + result.STA.subnet);
            $("#text-STA-set-sn").val(result.STA.subnet);
            $("#STA_mac").empty().append("MAC: " + result.STA.MAC);
            $("#text-STA-set-mac").val(result.STA.MAC);

            // DNS.. no functions in espWiFi lib to store it yet... 
            // $("#STA_dns").empty().append("DNS: " + result.DNS.subnet);
            // $("#text-DNS-set-sn").val(result.DNS.subnet);          

            if ($("#flip-dhcp").val() == "on") {
                $("#STAform :text").textinput('disable');
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

        }).success(function() { $("#status").empty().append("Connected").css("color", "green");  })
        .error(function() {   $("#status").empty().append("Not Connected").css("color", "red");  })
        .complete(function() {  });
    }

}); // end of wifipage create.. 

/****************************************************
 *                    About Page
 * 
 ****************************************************/
$(document).on("pagecreate", "#aboutpage", function() {
    //<!-- About page -->
    //var results; 
    //var staticwifi; 
    function GetAboutVars() {
        $.getJSON("data.esp?plain=AboutPage", function(results) {
            $("#aboutvars").empty();
            $("#aboutvars").append("<br>Version = " + results.version_var);
            $("#aboutvars").append("<br>Compile Date = " + results.compiletime_var);

            $("#aboutvars").append("<br>");

            $("#aboutvars").append("<br>Heap = " + results.heap_var);
            $("#aboutvars").append("<br>Millis = " + results.millis_var);
            $("#aboutvars").append("<br>UpTime = " + results.uptime_var);
            
            $("#aboutvars").append("<br>");

            $("#aboutvars").append("<br>Chip ID = " + results.chipid_var);
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

            $("#aboutvars").append("<br>Sketch Size = " + results.sketchsize_var);
            $("#aboutvars").append("<br>Free Space = " + results.freespace_var);
            $("#aboutvars").append("<br>");

            $("#aboutvars").append("<br>VCC = " + results.vcc_var);
            $("#aboutvars").append("<br>RSSI = " + results.rssi_var);
        }).success(function() { $("#status").empty().append("Connected").css("color", "green");  })
        .error(function() {   $("#status").empty().append("Not Connected").css("color", "red");  })
        .complete(function() {  });
    }

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


    function getAPvars() {

        $.getJSON("data.esp?plain=WiFiDetails", function(result) {
            
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
            }
            if ($("#flip-AP").val() == "off") {
                $("#APform :text").textinput('disable');
                $("#flip-AP-hidden").flipswitch('disable');
            }

        }).success(function() { $("#status").empty().append("Connected").css("color", "green");  })
        .error(function() {   $("#status").empty().append("Not Connected").css("color", "red");  })
        .complete(function() {  });

    }

    $("#apply_ap").click(function() {
        $.post("data.esp", $(this.form).serialize());
        $( "#apcollapse" ).collapsible( "collapse" );
        setTimeout(function() {
            getAPvars();            
        } , 2000);

    })

});


//}); // page init

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
;(function($, window, document, undefined)
{
    // Create the defaults once
    var pluginName = "fileUpload",
        defaults = {
            uploadData    : {},
            submitData    : {},
            uploadOptions : {},
            submitOptions : {},
            before        : function(){},
            beforeSubmit  : function(){ return true; },
            success       : function(){},
            error         : function(){},
            complete      : function(){}
        };

    // The actual plugin constructor
    function Plugin(element, options)
    {
        this.element    = element;
        this.$form      = $(element);
        this.$uploaders = $('input[type=file]', this.element);
        this.files      = {};
        this.settings   = $.extend({}, defaults, options);
        this._defaults  = defaults;
        this._name      = pluginName;
        this.init();
    }

    Plugin.prototype = 
    {
        /**
         * Initialize the plugin
         * 
         * @return void
         */
        init: function()
        {
            this.$uploaders.on('change', { context : this }, this.processFiles);
            this.$form.on('submit', { context : this }, this.uploadFiles);
        },



        /**
         * Process files after they are added
         * 
         * @param  jQuery event
         * @return void
         */
        processFiles: function(event)
        {
            var self = event.data.context;
            self.files[$(event.target).attr('name')] = event.target.files;
        },



        /**
         * Handles the file uploads
         * 
         * @param  jQuery event
         * @return void
         */
        uploadFiles: function(event)
        {
            event.stopPropagation(); // Stop stuff happening
            event.preventDefault(); // Totally stop stuff happening

            var self = event.data.context;

            // Run the before callback
            self.settings.before();

            // Declare a form data object
            var data = new FormData();
            data.append('file_upload_incoming', '1');

            // Add the files
            $.each(self.files, function(key, field)
            {
                $.each(field, function(key, value)
                {
                    data.append(key, value);
                });
            });

            // If there is uploadData passed append it
            $.each(self.settings.uploadData, function(key, value)
            {
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
                success: function(data, textStatus, jqXHR){ self.processSubmit(event, data); },
                error: function(jqXHR, textStatus, errorThrown){ self.settings.error(jqXHR, textStatus, errorThrown); }
            }, self.settings.uploadOptions));
        },



        /**
         * Submits form data with files
         * 
         * @param  jQuery event
         * @param  object
         * @return void
         */
        processSubmit: function(event, uploadData)
        {
            var self = event.data.context;

            // Run the beforeSubmit callback
            if(!self.settings.beforeSubmit(uploadData)) return;

            // Serialize the form data
            var data = self.$form.serializeArray();

            // Loop through the returned array from the server and add it to the new POST
            $.each(uploadData, function(key, value)
            {
                data.push({
                    'name'  : key,
                    'value' : value
                });
            });

            // If there is uploadData passed append it
            $.each(self.settings.submitData, function(key, value)
            {
                data.push({
                    'name'  : key,
                    'value' : value
                });
            });

            $.ajax($.extend({}, {
                url: self.$form.attr('action'),
                type: 'POST',
                data: data,
                cache: false,
                dataType: 'json',
                success: function(data, textStatus, jqXHR){ self.settings.success(data, textStatus, jqXHR); },
                error: function(jqXHR, textStatus, errorThrown){ self.settings.error(jqXHR, textStatus, errorThrown); },
                complete: function(jqXHR, textStatus){ self.settings.complete(jqXHR, textStatus); }
            }, self.settings.submitOptions));
        }
    };

    $.fn[pluginName] = function(options)
    {
        return this.each(function()
        {
            if(!$.data(this, "plugin_" + pluginName))
            {
                $.data(this, "plugin_" + pluginName, new Plugin(this, options));
            }
        });
    };

})(jQuery, window, document);








