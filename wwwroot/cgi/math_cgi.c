#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void math_op(char* buf)
{
    int x = -1,y = -1;
    sscanf(buf,"a=%d&b=%d", &x, &y);
    printf("%s",buf);
    printf("<html>\n");

    printf("<h1>%d + %d = %d</h1>", x, y, x + y);
    printf("<h1>%d - %d = %d</h1>", x, y, x - y);
    printf("<h1>%d * %d = %d</h1>", x, y, x * y);
    if(y != 0)
    printf("<h1>%d / %d = %d</h1>", x, y, x / y);
    printf("</html>\n");
}
int main()
{
    char buf[1024];

    if(getenv("METHOD"))
    {
        if(strcasecmp(getenv("METHOD"),"GET") == 0)
        {
            strcpy(buf,getenv("QUERY_STRING"));
        }
        else
        {
            int len = atoi(getenv("CONTENT_LENGTH"));
            int i = 0;
            for(;i < len;i++)
                read(0,buf+i,1);
        }
    }

    printf("result\n");
    math_op(buf);
    return 0;
}
