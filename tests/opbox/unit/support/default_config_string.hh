// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_DEFAULT_CONFIG_STRING_HH
#define FAODEL_DEFAULT_CONFIG_STRING_HH




std::string default_config_string = R"EOF(
  
)EOF";


std::string multitest_config_string = R"EOF(
# for finishSoft
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc
        
)EOF";



#endif //FAODEL_DEFAULT_SERIAL_SETTINGS_HH
