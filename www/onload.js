$(function () {

 $("[data-role='header']").toolbar();
 $("body>[data-role='panel']").panel();
 
 $("#popupDialogSPIFFS").enhanceWithin().popup();
 $("#popupDialogDeleteSettings").enhanceWithin().popup();
 $("#popupDialogClearWiFi").enhanceWithin().popup();

 $("#clearwifibutton").click(function () {
  $.post("/espman/data.esp", "resetwifi");
 });

 $("#formatspiffsbutton").click(function () {
  $.post("/espman/data.esp", "formatSPIFFS");
 });
 $("#deletesettingsbutton").click(function () {
  $.post("/espman/data.esp", "deletesettings");
 });

//});

//loadjs("/espman/espman.js", function () {
startEvents();
 getGenvars();
 $("body").css("visibility", "visible");
//});

//     });
//   });
 });
