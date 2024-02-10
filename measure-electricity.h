#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>

#include "../config/config.h"
#include "../config/enums.h"
#include "functions/enphaseFunction.h"
#include "functions/enphaseTokenRenew.h"
//#include "enphaseTokenRenew.h"
//#include "functions/froniusFunction.h"

extern Configmodule configmodule; 

void measureElectricity(void * parameter)
{
    for(;;)
    {
      if (!AP) 
      {
            if (configmodule.enphase_present ) 
            {     
                
                  Enphase_get();


            if(config.renewtoken == 1)
                  {

                  authenticateEnphase();
                  }
                  
            }
          
      }

      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }    
}

#endif
