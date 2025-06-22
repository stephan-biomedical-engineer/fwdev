#include "hal.h"
#include "app.h"

static char *app_name = 0;

char *main_get_app_name(void)
{
    return app_name;
}

int main(int argc, char *argv[])
{
    app_name = malloc(strlen(argv[0]) + 1);
    strcpy(app_name, argv[0]);

    hal_init();
    app_init();
    
    while(true)
    {
        bool run = app_loop();

        if(run == false)
            break;
    }

    app_deinit();
    hal_deinit();

    free(app_name);
    
    return 0;
}
