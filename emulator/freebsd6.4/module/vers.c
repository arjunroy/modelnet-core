#include <stdio.h>
#include "config.h"
void version_splash(void) ;
void version_splash() {
        printf ("%s %s   built %s %s cosmin@evgsics9 for 6.2-RELEASE\n",
		PACKAGE_NAME, VERSION, __DATE__,__TIME__);
}
