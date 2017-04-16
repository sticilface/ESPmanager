/*! \file 
    \brief to come
    
    This is to test the documentation.
*/

#pragma once


extern "C" {
#include <user_interface.h>
}


extern "C" { 
  void custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end ); 
}

