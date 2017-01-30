#include "SaveStack.h"
#include <FS.h>



void custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end )
{

	File f = SPIFFS.open("/crash.log", "w");

	if (f) {

		if (rst_info->reason != 0) {
			char buff[200];
			sprintf(&buff[0], "Fatal exception:(%d) \nflag:%d (%s) epc1:0x%08x epc2:0x%08x epc3:0x%08x excvaddr:0x%08x depc:0x%08x\n", rst_info->exccause, rst_info->reason, (rst_info->reason == 0 ? "DEFAULT" : rst_info->reason == 1 ? "WDT" : rst_info->reason == 2 ? "EXCEPTION" : rst_info->reason == 3 ? "SOFT_WDT" : rst_info->reason == 4 ? "SOFT_RESTART" : rst_info->reason == 5 ? "DEEP_SLEEP_AWAKE" : rst_info->reason == 6 ? "EXT_SYS_RST" : "???"), rst_info->epc1, rst_info->epc2, rst_info->epc3, rst_info->excvaddr, rst_info->depc);
			f.println(buff);
		}

		f.printf("start: %08x end: %08x\n", stack, stack_end);

		f.print("\n>>>stack>>>\n");

		char stackline[46];
		int count = 0;

		for (uint32_t pos = stack; pos < stack_end; pos += 0x10) {
			count++;
			uint32_t* values = (uint32_t*)(pos);
			bool looksLikeStackFrame = (values[2] == pos + 0x10);
			sprintf(stackline, "%08x:  %08x %08x %08x %08x %c", pos, values[0], values[1], values[2], values[3], (looksLikeStackFrame) ? '<' : ' ');
			f.println(stackline);
			if (count > 100) { break; }
		}

		f.print("<<<stack<<<\n");

		f.close();

		Serial.println("STACK SAVE DONE");

	}

}